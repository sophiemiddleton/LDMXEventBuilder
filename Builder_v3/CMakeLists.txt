# Minimum required version for CMake
cmake_minimum_required(VERSION 3.10)

# Define the project name
project(event_builder)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the output directory for all executables to the 'bin' folder
# inside the SOURCE directory (project root)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Create an executable named 'event_builder'
# It's built from all the source files.
add_executable(event_builder
    src/main.cc
)

# Add the 'include' directory to the include search path
target_include_directories(event_builder PUBLIC include)
