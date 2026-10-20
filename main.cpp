#include "crow.h"
#include <curl/curl.h>
#include <cctype>
#include <cstdlib>
#include <string>

struct SupabaseConfig {
    std::string url;      // e.g. https://YOUR_PROJECT.supabase.co
    std::string anon_key;
};

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// Sends a GET to the Supabase REST API. Returns the HTTP status (0 if unreachable).
static long supabase_get(const SupabaseConfig& cfg, const std::string& path_and_query, std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) return 0;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("apikey: " + cfg.anon_key).c_str());
    headers = curl_slist_append(headers, ("Authorization: Bearer " + cfg.anon_key).c_str());

    std::string url = cfg.url + path_and_query;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    long status = 0;
    if (curl_easy_perform(curl) == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return status;
}

static crow::response json_error(int code, const std::string& message) {
    crow::response res;
    res.code = code;
    res.set_header("Content-Type", "application/json");
    res.body = "{\"error\":\"" + message + "\"}";
    return res;
}

int main() {
    const char* url = std::getenv("SUPABASE_URL");
    const char* key = std::getenv("SUPABASE_ANON_KEY");
    if (!url || !key) {
        fprintf(stderr, "Set SUPABASE_URL and SUPABASE_ANON_KEY first.\n");
        return 1;
    }
    SupabaseConfig cfg{url, key};

    curl_global_init(CURL_GLOBAL_DEFAULT);

    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([]() {
        return "Hello from CMPE-320!";
    });

    // GET /classes            -> all classes
    // GET /classes?name=calc  -> classes whose name contains "calc"
    CROW_ROUTE(app, "/classes")([&cfg](const crow::request& req) {
        std::string term = req.url_params.get("name") ? req.url_params.get("name") : "";

        // Trim surrounding spaces
        auto first = term.find_first_not_of(' ');
        term = (first == std::string::npos) ? "" : term.substr(first, term.find_last_not_of(' ') - first + 1);

        if (term.size() > 100) {
            return json_error(400, "Search term too long");
        }
        // Allow only letters, digits, spaces, and hyphens. This blocks SQL/PostgREST wildcards and operators.
        for (char c : term) {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-')) {
                return json_error(400, "Search may only contain letters, numbers, spaces, and hyphens");
            }
        }

        std::string path = "/rest/v1/classes?select=id,code,name,instructor,term&order=name.asc";
        if (!term.empty()) {
            // PostgREST uses * as the wildcard for ILIKE
            char* enc = curl_easy_escape(nullptr, ("*" + term + "*").c_str(), 0);
            path += "&name=ilike." + std::string(enc);
            curl_free(enc);
        }

        std::string body;
        long status = supabase_get(cfg, path, body);
        if (status == 0) {
            return json_error(502, "Could not reach database");
        }

        crow::response res;
        res.code = static_cast<int>(status);
        res.set_header("Content-Type", "application/json");
        res.body = body;
        return res;
    });

    // GET /classes/:id
    CROW_ROUTE(app, "/classes/<int>")([&cfg](int id) {
        std::string path = "/rest/v1/classes?select=*&id=eq." + std::to_string(id);

        std::string body;
        long status = supabase_get(cfg, path, body);
        if (status == 0) {
            return json_error(502, "Could not reach database");
        }
        // PostgREST returns a list, so return the first item or 404
        if (body == "[]") {
            return json_error(404, "Class not found");
        }

        crow::response res;
        res.code = static_cast<int>(status);
        res.set_header("Content-Type", "application/json");
        res.body = body;
        return res;
    });

    // Runs on PORT if set, else 8080
    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::stoi(port_env) : 8080;

    app.port(port).multithreaded().run();

    curl_global_cleanup();
    return 0;
}