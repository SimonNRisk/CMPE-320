#include "crow.h"
#include <cstdlib>

int main() {
    // Declares a simple app with no middleware
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([]() {
        return "Hello from CMPE-320!";
    });

    // Runs the port on a PORT env if set, else 8080
    int port = std::stoi(std::getenv("PORT") 
                         ? std::getenv("PORT") 
                         : "8080");

    app.port(port).multithreaded().run();
}