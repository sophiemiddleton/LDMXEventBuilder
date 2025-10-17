# Minimum required version for CMake

cmake_minimum_required(VERSION 3.10)

# Define the project name
project(simpleEB)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

FIND_PACKAGE(Boost)

# Create an executable named 'MyProject'
# It's built from all the source files.
add_executable(simpleEB
    src/main_tcp.cc
)
# Add the 'include' directory to the include search path
target_include_directories(simpleEB PUBLIC include)
