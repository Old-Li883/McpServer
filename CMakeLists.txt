cmake_minimum_required(VERSION 3.15)
project(McpServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含 FetchContent 模块
include(FetchContent)

# 自动下载 nlohmann/json


FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# 头文件目录
include_directories(${CMAKE_SOURCE_DIR}/config)

# 源文件
set(SOURCES
    src/main.cpp
    config/Config.cpp
)

# 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接 nlohmann/json
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)