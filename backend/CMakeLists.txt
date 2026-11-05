cmake_minimum_required(VERSION 3.20)
project(cpp_backend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.1
)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.1
)

FetchContent_Declare(
    argon2
    GIT_REPOSITORY https://github.com/P-H-C/phc-winner-argon2.git
    GIT_TAG 20190702
)

FetchContent_MakeAvailable(Crow jwt-cpp argon2)

# argon2 has no CMake build of its own
find_package(Threads REQUIRED)
add_library(argon2_lib STATIC
    ${argon2_SOURCE_DIR}/src/argon2.c
    ${argon2_SOURCE_DIR}/src/core.c
    ${argon2_SOURCE_DIR}/src/encoding.c
    ${argon2_SOURCE_DIR}/src/ref.c
    ${argon2_SOURCE_DIR}/src/thread.c
    ${argon2_SOURCE_DIR}/src/blake2/blake2b.c
)
target_include_directories(argon2_lib PUBLIC ${argon2_SOURCE_DIR}/include)
target_include_directories(argon2_lib PRIVATE ${argon2_SOURCE_DIR}/src)
target_link_libraries(argon2_lib PUBLIC Threads::Threads)

add_executable(server main.cpp supabase.cpp classes.cpp resources.cpp auth.cpp)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
target_link_libraries(server PRIVATE Crow::Crow CURL::libcurl jwt-cpp::jwt-cpp argon2_lib OpenSSL::SSL OpenSSL::Crypto)