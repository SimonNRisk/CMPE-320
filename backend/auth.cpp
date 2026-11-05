#include "auth.h"
#include <argon2.h>
#include <curl/curl.h>
#include <jwt-cpp/jwt.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <exception>
#include <stdexcept>
#include <vector>

static const char* const ISSUER = "cmpe320";
static const int ACCESS_TOKEN_SECONDS = 15 * 60;
static const int REFRESH_TOKEN_DAYS = 30;
static const size_t MIN_PASSWORD = 12;
static const size_t MAX_PASSWORD = 128;
static const size_t MAX_EMAIL = 254;
static const size_t MAX_PROFILE_TEXT = 100;   // school, program
static const size_t MAX_COURSES = 20;
static const size_t MAX_COURSE_CODE = 20;

// Argon2id settings: 3 passes over 64 MiB. Changing these only affects new
// hashes, since each stored hash records the settings it was made with.
static const uint32_t ARGON2_TIME = 3;
static const uint32_t ARGON2_MEMORY_KIB = 64 * 1024;
static const uint32_t ARGON2_THREADS = 1;
static const size_t SALT_BYTES = 16;
static const size_t HASH_BYTES = 32;

//  Crypto helpers
static std::vector<unsigned char> random_bytes(size_t count)
{
    std::vector<unsigned char> out(count);
    if (RAND_bytes(out.data(), static_cast<int>(count)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return out;
}

static std::string to_hex(const unsigned char* data, size_t len)
{
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0x0f];
    }
    return out;
}

static std::string sha256_hex(const std::string& s)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (EVP_Digest(s.data(), s.size(), hash, &len, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("SHA-256 failed");
    }
    return to_hex(hash, len);
}

static std::string hash_password(const std::string& password)
{
    std::vector<unsigned char> salt = random_bytes(SALT_BYTES);
    std::string encoded(argon2_encodedlen(ARGON2_TIME, ARGON2_MEMORY_KIB, ARGON2_THREADS,
                                          SALT_BYTES, HASH_BYTES, Argon2_id), '\0');
    int result = argon2id_hash_encoded(ARGON2_TIME, ARGON2_MEMORY_KIB, ARGON2_THREADS,
                                       password.data(), password.size(), salt.data(), salt.size(),
                                       HASH_BYTES, &encoded[0], encoded.size());
    if (result != ARGON2_OK) {
        throw std::runtime_error(argon2_error_message(result));
    }
    encoded.resize(encoded.find('\0'));
    return encoded;
}

static bool verify_password(const std::string& encoded, const std::string& password)
{
    return argon2id_verify(encoded.c_str(), password.data(), password.size()) == ARGON2_OK;
}

static std::string create_access_token(long long user_id, const std::string& secret)
{
    auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_type("JWT")
        .set_issuer(ISSUER)
        .set_subject(std::to_string(user_id))
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::seconds(ACCESS_TOKEN_SECONDS))
        .sign(jwt::algorithm::hs256{secret});
}

//  Request helpers
static bool normalize_email(std::string& email)
{
    if (email.empty() || email.size() > MAX_EMAIL) return false;
    for (char& ch : email) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c <= 32 || c >= 127) return false;
        ch = static_cast<char>(std::tolower(c));
    }
    auto at = email.find('@');
    return at != std::string::npos && at != 0 && at + 1 != email.size()
        && email.find('@', at + 1) == std::string::npos;
}

// 3 to 32 characters: letters, digits, '_', '.' or '-'
static bool valid_username(const std::string& name)
{
    if (name.size() < 3 || name.size() > 32) return false;
    return std::all_of(name.begin(), name.end(), [](char ch) {
        unsigned char c = static_cast<unsigned char>(ch);
        return std::isalnum(c) || c == '_' || c == '.' || c == '-';
    });
}

// Reads a required text field from a small JSON body. Returns false if it's missing.
static bool read_field(const crow::request& req, const char* name, std::string& out)
{
    if (req.body.size() > 10000) return false;
    auto json = crow::json::load(req.body);
    if (!json || json.t() != crow::json::type::Object) return false;
    if (!json.has(name) || json[name].t() != crow::json::type::String) return false;
    out = json[name].s();
    return true;
}

// Current time plus an offset, in the UTC format Postgres accepts
static std::string utc_timestamp(std::chrono::seconds offset)
{
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + offset);
    std::tm utc{};
    gmtime_r(&t, &utc);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}

static crow::response database_error(const char* action, long status, const std::string& body)
{
    if (status == 0) return json_error(502, "Could not reach database");
    // Log the details privately instead of sending them to the user
    fprintf(stderr, "%s failed (HTTP %ld): %s\n", action, status, body.c_str());
    return json_error(502, "Could not complete request");
}

// Saves a new refresh token and returns both tokens to the client
static crow::response issue_tokens(const SupabaseConfig& cfg, const std::string& secret, long long user_id)
{
    std::vector<unsigned char> raw = random_bytes(32);
    std::string refresh_token = to_hex(raw.data(), raw.size());

    crow::json::wvalue row;
    row["token_hash"] = sha256_hex(refresh_token);
    row["user_id"] = user_id;
    row["expires_at"] = utc_timestamp(std::chrono::hours(24 * REFRESH_TOKEN_DAYS));

    std::string response;
    long status = supabase_post(cfg, "/rest/v1/refresh_tokens", row.dump(), response);
    if (status != 201) return database_error("Saving refresh token", status, response);

    crow::json::wvalue out;
    out["access_token"] = create_access_token(user_id, secret);
    out["refresh_token"] = refresh_token;
    out["token_type"] = "Bearer";
    out["expires_in"] = ACCESS_TOKEN_SECONDS;

    crow::response res;
    res.code = 200;
    res.set_header("Content-Type", "application/json");
    res.body = out.dump();
    return res;
}

//  Public API
std::optional<long long> authenticate(const crow::request& req, const std::string& jwt_secret)
{
    static const std::string PREFIX = "Bearer ";
    const std::string& header = req.get_header_value("Authorization");
    if (header.compare(0, PREFIX.size(), PREFIX) != 0) return std::nullopt;

    try {
        auto decoded = jwt::decode(header.substr(PREFIX.size()));
        jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{jwt_secret})
            .with_issuer(ISSUER)
            .verify(decoded);
        return std::stoll(decoded.get_subject());
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void register_auth_routes(crow::SimpleApp& app, const SupabaseConfig& cfg, const std::string& jwt_secret)
{
    CROW_ROUTE(app, "/auth/register").methods(crow::HTTPMethod::Post)
    ([&cfg](const crow::request& req) {
        std::string username, email, password;
        if (!read_field(req, "username", username) || !read_field(req, "email", email)
            || !read_field(req, "password", password)) {
            return json_error(400, "Body must include text fields: username, email and password");
        }
        if (!valid_username(username)) {
            return json_error(400, "username must be 3 to 32 letters, digits, '_', '.' or '-'");
        }
        if (!normalize_email(email)) {
            return json_error(400, "email is not valid");
        }
        if (password.size() < MIN_PASSWORD || password.size() > MAX_PASSWORD) {
            return json_error(400, "password must be 12 to 128 characters");
        }

        // Arguments for the register_user database function (see the create_auth migration)
        crow::json::wvalue args;
        args["p_username"] = username;
        args["p_email"] = email;

        // Optional profile fields; anything left out stays empty until the user fills it in
        auto json = crow::json::load(req.body);
        for (const char* field : {"school", "program"}) {
            if (!json.has(field)) continue;
            if (json[field].t() != crow::json::type::String || json[field].s().size() > MAX_PROFILE_TEXT) {
                return json_error(400, std::string(field) + " must be text of at most 100 characters");
            }
            args[std::string("p_") + field] = std::string(json[field].s());
        }
        if (json.has("year")) {
            if (json["year"].t() != crow::json::type::Number || json["year"].nt() == crow::json::num_type::Floating_point
                || json["year"].i() < 1 || json["year"].i() > 5) {
                return json_error(400, "year must be a whole number from 1 to 5");
            }
            args["p_year"] = json["year"].i();
        }
        if (json.has("courses")) {
            const auto& list = json["courses"];
            if (list.t() != crow::json::type::List || list.size() > MAX_COURSES) {
                return json_error(400, "courses must be a list of at most 20 course codes");
            }
            std::vector<std::string> courses;
            for (const auto& c : list) {
                if (c.t() != crow::json::type::String || c.s().size() == 0 || c.s().size() > MAX_COURSE_CODE) {
                    return json_error(400, "each course must be a code of 1 to 20 characters");
                }
                courses.push_back(c.s());
            }
            args["p_courses"] = courses;
        }

        // Hash last, since it is the slow step
        args["p_password_hash"] = hash_password(password);

        std::string response;
        long status = supabase_post(cfg, "/rest/v1/rpc/register_user", args.dump(), response);
        if (status == 409) {
            if (response.find("users_username_key") != std::string::npos) {
                return json_error(409, "That username is taken");
            }
            return json_error(409, "An account with this email already exists");
        }
        if (status != 200 && status != 204) return database_error("Creating user", status, response);

        crow::response res;
        res.code = 201;
        res.set_header("Content-Type", "application/json");
        res.body = "{\"status\":\"created\"}";
        return res;
    });

    CROW_ROUTE(app, "/auth/login").methods(crow::HTTPMethod::Post)
    ([&cfg, &jwt_secret](const crow::request& req) {
        // Checked when the email is unknown, so that case takes as long as a wrong password
        static const std::string DUMMY_HASH = hash_password("not-a-real-account-password");

        std::string email, password;
        if (!read_field(req, "email", email) || !read_field(req, "password", password)) {
            return json_error(400, "Body must include text fields: email and password");
        }

        bool valid_email = normalize_email(email) && password.size() <= MAX_PASSWORD;
        std::string body = "[]";
        if (valid_email) {
            char* enc = curl_easy_escape(nullptr, email.c_str(), static_cast<int>(email.size()));
            if (!enc) return json_error(500, "Could not encode email");
            std::string path = std::string("/rest/v1/user_auth?select=user_id,password_hash&email=eq.") + enc;
            curl_free(enc);

            body.clear();
            long status = supabase_get(cfg, path, body);
            if (status != 200) return database_error("Finding user", status, body);
        }

        auto rows = crow::json::load(body);
        if (!rows || rows.t() != crow::json::type::List) {
            return database_error("Reading user", 200, "unexpected response");
        }
        if (rows.size() == 0) {
            if (password.size() <= MAX_PASSWORD) verify_password(DUMMY_HASH, password);
            return json_error(401, "Invalid email or password");
        }

        std::string stored = rows[0]["password_hash"].s();
        if (!verify_password(stored, password)) {
            return json_error(401, "Invalid email or password");
        }
        return issue_tokens(cfg, jwt_secret, rows[0]["user_id"].i());
    });

    CROW_ROUTE(app, "/auth/refresh").methods(crow::HTTPMethod::Post)
    ([&cfg, &jwt_secret](const crow::request& req) {
        std::string token;
        if (!read_field(req, "refresh_token", token)) {
            return json_error(400, "Body must include text field: refresh_token");
        }

        // Deleting and reading in one step means a token can never be used twice,
        // even if two requests race.
        std::string body;
        long status = supabase_delete(cfg, "/rest/v1/refresh_tokens?token_hash=eq." + sha256_hex(token)
                                           + "&expires_at=gt." + utc_timestamp(std::chrono::seconds(0)), body);
        if (status != 200) return database_error("Using refresh token", status, body);

        auto rows = crow::json::load(body);
        if (!rows || rows.t() != crow::json::type::List) {
            return database_error("Reading refresh token", 200, "unexpected response");
        }
        if (rows.size() == 0) return json_error(401, "Invalid refresh token");
        return issue_tokens(cfg, jwt_secret, rows[0]["user_id"].i());
    });

    CROW_ROUTE(app, "/auth/logout").methods(crow::HTTPMethod::Post)
    ([&cfg](const crow::request& req) {
        std::string token;
        if (!read_field(req, "refresh_token", token)) {
            return json_error(400, "Body must include text field: refresh_token");
        }

        std::string body;
        long status = supabase_delete(cfg, "/rest/v1/refresh_tokens?token_hash=eq." + sha256_hex(token), body);
        if (status != 200) return database_error("Revoking refresh token", status, body);
        return crow::response(204);
    });

    CROW_ROUTE(app, "/auth/me")
    ([&cfg, &jwt_secret](const crow::request& req) {
        std::optional<long long> user_id = authenticate(req, jwt_secret);
        if (!user_id) return json_error(401, "Sign in required");

        std::string body;
        // The email lives in user_auth; the profile is joined in through the shared id
        long status = supabase_get(cfg, "/rest/v1/user_auth?select=email,"
                                        "profile:users(id,username,school,year,program,courses,created_at)"
                                        "&user_id=eq." + std::to_string(*user_id), body);
        if (status != 200) return database_error("Finding user", status, body);

        auto rows = crow::json::load(body);
        if (!rows || rows.t() != crow::json::type::List) {
            return database_error("Reading user", 200, "unexpected response");
        }
        if (rows.size() == 0 || rows[0]["profile"].t() != crow::json::type::Object) {
            return json_error(401, "Sign in required");
        }

        crow::json::wvalue out = rows[0]["profile"];
        out["email"] = std::string(rows[0]["email"].s());

        crow::response res;
        res.code = 200;
        res.set_header("Content-Type", "application/json");
        res.body = out.dump();
        return res;
    });
}
