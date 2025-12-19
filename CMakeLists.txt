cmake_minimum_required(VERSION 3.10)
project(ZookeeperKVStore VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compile commands for Neovim
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Locate the ZooKeeper Library
# 'zookeeper_mt' is the multi-threaded version of the C client
find_library(ZOOKEEPER_LIB zookeeper_mt)

if(NOT ZOOKEEPER_LIB)
    message(FATAL_ERROR "ZooKeeper library not found! Please install libzookeeper-mt-dev.")
endif()

# 2. Define the Executable
add_executable(kv_server src/main.cpp)

# 3. Include directories (where your .h files are)
target_include_directories(kv_server PRIVATE ${PROJECT_SOURCE_DIR}/src)

# 4. Link the ZooKeeper library to your executable
target_link_libraries(kv_server PRIVATE ${ZOOKEEPER_LIB})
