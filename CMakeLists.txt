cmake_minimum_required(VERSION 3.10)
project(boogie_vec_server VERSION 0.1 LANGUAGES CXX)

# Choose C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler warnings
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Include directories
include_directories(third_party)

# Add executable
add_executable(boogie_vec_server
  src/main.cpp
  src/server.cpp
  src/snapshot_io.cpp
  src/knn_bruteforce.cpp
  src/util.cpp
)

# If using threads (recommended for production), link pthread on unix
find_package(Threads REQUIRED)
target_link_libraries(boogie_vec_server PRIVATE Threads::Threads)

# Install target (optional)
install(TARGETS boogie_vec_server DESTINATION bin)