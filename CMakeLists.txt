cmake_minimum_required(VERSION 3.16)

project(uniConnect
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(uniconnect
    src/main.cpp
    src/server/Server.cpp
)

target_include_directories(uniconnect PRIVATE
    include
)