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

# 查找 spdlog（优先使用系统安装的）
find_package(spdlog QUIET)

# 如果系统没有安装，使用 FetchContent 下载
if(NOT spdlog_FOUND)
    message(STATUS "spdlog not found, using FetchContent...")
    FetchContent_Declare(
        spdlog_fc
        URL https://github.com/gabime/spdlog/releases/download/v1.12.0/spdlog-1.12.0.tgz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog_fc)
    set(SPDLOG_TARGET spdlog::spdlog_header_only)
else()
    message(STATUS "spdlog found system installation")
    set(SPDLOG_TARGET spdlog::spdlog)
endif()

# 头文件目录
include_directories(${CMAKE_SOURCE_DIR}/config)
include_directories(${CMAKE_SOURCE_DIR}/src/logger)

# 源文件
set(SOURCES
    src/main.cpp
    config/Config.cpp
    src/logger/Logger.cpp
)

# 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接依赖库
target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    ${SPDLOG_TARGET}
)

# 测试可执行文件
add_executable(test_logger tests/test_logger.cpp src/logger/Logger.cpp)
target_link_libraries(test_logger PRIVATE ${SPDLOG_TARGET})