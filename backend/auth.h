#ifndef AUTH_H
#define AUTH_H

#include <optional>
#include <string>
#include "crow.h"
#include "supabase.h"

// Returns the user ID from a valid "Authorization: Bearer <access token>" header.
// Use this at the start of any route that should require a signed-in user.
std::optional<long long> authenticate(const crow::request& req, const std::string& jwt_secret);

// Adds the auth routes to the app. `cfg` must hold the service-role key.
//   POST /auth/register  {"username","email","password"} plus optional
//                        "school","program" (text), "year" (1-5), "courses" (list of codes)
//                                              -> create an account
//   POST /auth/login     {"email","password"}  -> access and refresh tokens
//   POST /auth/refresh   {"refresh_token"}     -> new tokens; the old one stops working
//   POST /auth/logout    {"refresh_token"}     -> revoke a refresh token
//   GET  /auth/me        Bearer token          -> the signed-in user's profile and email
void register_auth_routes(crow::SimpleApp& app, const SupabaseConfig& cfg, const std::string& jwt_secret);

#endif // AUTH_H
