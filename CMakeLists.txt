cmake_minimum_required(VERSION 3.15)
project(McpServer)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 构建类型（如果没有指定）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 输出信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")

# 包含 FetchContent 模块
include(FetchContent)

# 查找 nlohmann/json（优先使用系统安装的）
find_package(nlohmann_json 3.2.0 QUIET)

# 如果系统没有安装，使用 FetchContent 下载
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, using FetchContent...")
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CONFIGURE_COMMAND "" # 跳过配置，因为这是 header-only 库
        BUILD_COMMAND ""     # 跳过构建
    )
    FetchContent_MakeAvailable(json)
else()
    message(STATUS "nlohmann_json found system installation")
endif()

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
    # 设置 spdlog 选项
    set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog_fc)
    set(SPDLOG_TARGET spdlog::spdlog_header_only)
else()
    message(STATUS "spdlog found system installation")
    set(SPDLOG_TARGET spdlog::spdlog)
endif()

# 查找 Google Test（优先使用系统安装的）
find_package(GTest QUIET)

# 如果系统没有安装，使用 FetchContent 下载
if(NOT GTest_FOUND)
    message(STATUS "GTest not found, using FetchContent...")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # 设置 GTest 选项，避免生成器冲突
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_Threads ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
else()
    message(STATUS "GTest found system installation")
endif()


# 头文件目录
include_directories(${CMAKE_SOURCE_DIR}/config)
include_directories(${CMAKE_SOURCE_DIR}/src/logger)
include_directories(${CMAKE_SOURCE_DIR}/src/json_rpc)

# 源文件
set(SOURCES
    src/main.cpp
    config/Config.cpp
    src/logger/Logger.cpp
    src/json_rpc/jsonrpc_serialization.cpp
    src/json_rpc/jsonrpc.cpp
    src/json_rpc/stdio_jsonrpc.cpp
)

# 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接依赖库
target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    ${SPDLOG_TARGET}
)

# 链接 GTest 的辅助函数
macro(link_gtest target)
    if(GTest_FOUND)
        target_link_libraries(${target} PRIVATE GTest::gtest GTest::gtest_main)
    else()
        target_link_libraries(${target} PRIVATE gtest gtest_main)
    endif()
endmacro()

# 测试可执行文件
add_executable(test_logger tests/test_logger.cpp src/logger/Logger.cpp)
target_link_libraries(test_logger PRIVATE ${SPDLOG_TARGET})
link_gtest(test_logger)

add_executable(test_jsonrpc tests/test_jsonrpc.cpp
    src/json_rpc/jsonrpc_serialization.cpp
    src/json_rpc/jsonrpc.cpp
)
target_link_libraries(test_jsonrpc PRIVATE nlohmann_json::nlohmann_json)
link_gtest(test_jsonrpc)

# stdio_jsonrpc 测试
add_executable(test_stdio_jsonrpc tests/test_stdio_jsonrpc.cpp
    src/json_rpc/jsonrpc_serialization.cpp
    src/json_rpc/jsonrpc.cpp
    src/json_rpc/stdio_jsonrpc.cpp
    src/logger/Logger.cpp
    config/Config.cpp
)
target_link_libraries(test_stdio_jsonrpc PRIVATE
    nlohmann_json::nlohmann_json
    ${SPDLOG_TARGET}
)
link_gtest(test_stdio_jsonrpc)

# 启用测试发现
include(GoogleTest)
gtest_discover_tests(test_logger)
gtest_discover_tests(test_jsonrpc)
gtest_discover_tests(test_stdio_jsonrpc)

# 为 CLion 设置
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)