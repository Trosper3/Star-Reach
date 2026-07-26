cmake_minimum_required(VERSION 3.20)
project(StarReach VERSION 0.1.0 LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories (Force all build types directly into /bin without subfolders)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# 1. DEFINE THE TARGET FIRST
add_executable(StarReach
    src/main.cpp
)

# 2. Set include directories for the target
target_include_directories(StarReach PRIVATE src)

# 3. Find packages downloaded by vcpkg
find_package(EnTT CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 4. Link them to the target
target_link_libraries(StarReach PRIVATE 
    EnTT::EnTT 
    nlohmann_json::nlohmann_json
)