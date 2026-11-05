#include "crow.h"
#include <curl/curl.h>
#include <cstdio>
#include <cstdlib>
#include <string>
 
#include "supabase.h"
#include "classes.h"
#include "resources.h"
#include "auth.h"
 
int main()
{
    const char* url = std::getenv("SUPABASE_URL");
    const char* key = std::getenv("SUPABASE_ANON_KEY");
 
    if (!url || !key) {
        fprintf(stderr, "Set SUPABASE_URL and SUPABASE_ANON_KEY first.\n");
        return 1;
    }
    SupabaseConfig cfg{url, key};

    const char* service_key = std::getenv("SUPABASE_SERVICE_ROLE_KEY");
    const char* jwt_secret_env = std::getenv("JWT_SECRET");

    if (!service_key || !jwt_secret_env) {
        fprintf(stderr, "Set SUPABASE_SERVICE_ROLE_KEY and JWT_SECRET first.\n");
        return 1;
    }
    std::string jwt_secret = jwt_secret_env;
    if (jwt_secret.size() < 32) {
        fprintf(stderr, "JWT_SECRET must be at least 32 characters.\n");
        return 1;
    }
    // Only the auth routes use the service-role key; everything else stays on the anon key
    SupabaseConfig auth_cfg{url, service_key};

    curl_global_init(CURL_GLOBAL_DEFAULT);
 
    crow::SimpleApp app;
 
    CROW_ROUTE(app, "/")([]() {
        return "Hello from CMPE-320!";
    });
 
    // GET /classes, GET /classes/<id>   (search options live in classes.cpp)
    register_class_routes(app, cfg);
 
    // GET and POST /classes/<id>/resources   (see resources.cpp)
    register_resource_routes(app, cfg);

    // /auth/register, /auth/login, /auth/refresh, /auth/logout, /auth/me   (see auth.h)
    register_auth_routes(app, auth_cfg, jwt_secret);
 
    // Runs on PORT if set, else 8080
    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::stoi(port_env) : 8080;
 
    app.port(port).multithreaded().run();
 
    curl_global_cleanup();
    return 0;
}
