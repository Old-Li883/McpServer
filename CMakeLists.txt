cmake_minimum_required(VERSION 3.15)
project(McpServer)

# 强制使用 Homebrew 的库，避免与 miniconda3 冲突
# 设置 CMake 查找路径优先级
set(CMAKE_PREFIX_PATH "/opt/homebrew/lib/cmake" ${CMAKE_PREFIX_PATH})
set(CMAKE_INCLUDE_PATH "/opt/homebrew/include" ${CMAKE_INCLUDE_PATH})
set(CMAKE_LIBRARY_PATH "/opt/homebrew/lib" ${CMAKE_LIBRARY_PATH})

# 从 CMAKE_PREFIX_PATH 中移除 miniconda3 路径
if(CMAKE_PREFIX_PATH)
    list(FILTER CMAKE_PREFIX_PATH EXCLUDE REGEX "miniconda3")
endif()
if(CMAKE_INCLUDE_PATH)
    list(FILTER CMAKE_INCLUDE_PATH EXCLUDE REGEX "miniconda3")
endif()
if(CMAKE_LIBRARY_PATH)
    list(FILTER CMAKE_LIBRARY_PATH EXCLUDE REGEX "miniconda3")
endif()

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

# ========== 依赖库配置 ==========

# 查找 nlohmann/json（优先使用系统安装的）
find_package(nlohmann_json 3.2.0 QUIET)

# 如果系统没有安装，使用 FetchContent 下载
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, using FetchContent...")
    include(FetchContent)
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

# 查找 cpp-httplib（优先使用系统安装的）
find_package(httplib QUIET)

# 如果系统没有安装，使用 FetchContent 下载
if(NOT httplib_FOUND)
    message(STATUS "cpp-httplib not found, using FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.15.3
    )
    # 设置 cpp-httplib 选项
    set(HTTPLIB_COMPILE ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(httplib)
else()
    message(STATUS "cpp-httplib found system installation (version ${httplib_VERSION})")
endif()

# ========== 强制静态链接 spdlog 和 fmt ==========
# 这是解决版本兼容性问题的关键：直接使用静态库

message(STATUS "Using static linking for spdlog and fmt to ensure compatibility")

# 检查静态库是否存在
set(SPDLOG_STATIC_LIB "/opt/homebrew/lib/libspdlog.a")
set(FMT_STATIC_LIB "/opt/homebrew/lib/libfmt.a")

if(NOT EXISTS "${SPDLOG_STATIC_LIB}")
    message(FATAL_ERROR "spdlog static library not found at ${SPDLOG_STATIC_LIB}")
endif()

if(NOT EXISTS "${FMT_STATIC_LIB}")
    message(FATAL_ERROR "fmt static library not found at ${FMT_STATIC_LIB}")
endif()

message(STATUS "Found spdlog static library: ${SPDLOG_STATIC_LIB}")
message(STATUS "Found fmt static library: ${FMT_STATIC_LIB}")

# ========== 查找 Google Test（可选，仅用于测试） ==========
find_package(GTest QUIET)

# 如果系统没有安装，使用 FetchContent 下载
if(NOT GTest_FOUND)
    message(STATUS "GTest not found, using FetchContent...")
    include(FetchContent)
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

# ========== 头文件目录 ==========
include_directories(${CMAKE_SOURCE_DIR}/config)
include_directories(${CMAKE_SOURCE_DIR}/src/logger)
include_directories(${CMAKE_SOURCE_DIR}/src/json_rpc)
include_directories(${CMAKE_SOURCE_DIR}/src/mcp)
include_directories(/opt/homebrew/include)

# ========== 源文件 ==========
set(SOURCES
    src/main.cpp
    config/Config.cpp
    src/logger/Logger.cpp
    src/json_rpc/jsonrpc_serialization.cpp
    src/json_rpc/jsonrpc.cpp
    src/json_rpc/stdio_jsonrpc.cpp
    src/json_rpc/http_jsonrpc_server.cpp
    src/mcp/ToolManager.cpp
    src/mcp/ResourceManager.cpp
    src/mcp/PromptManager.cpp
    src/mcp/McpServer.cpp
    src/mcp/McpServerRunner.cpp
)

# ========== 可执行文件 ==========
add_executable(${PROJECT_NAME} ${SOURCES})

# ========== 链接依赖库 ==========
# 主程序链接：强制静态链接 spdlog 和 fmt
target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    ${SPDLOG_STATIC_LIB}
    ${FMT_STATIC_LIB}
    httplib::httplib
)

# ========== 查找和链接 macOS 系统框架 ==========
find_library(FOUNDATION_LIBRARY Foundation)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(SECURITY_LIBRARY Security)
find_library(CFNETWORK_LIBRARY CFNetwork)

if(NOT FOUNDATION_LIBRARY)
    message(FATAL_ERROR "Foundation framework not found")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${FOUNDATION_LIBRARY}
    ${COREFOUNDATION_LIBRARY}
    ${SECURITY_LIBRARY}
    ${CFNETWORK_LIBRARY}
)

# ========== 链接 OpenSSL 和 Zlib ==========
find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
)

# ========== 链接 GTest 的辅助函数 ==========
macro(link_gtest target)
    if(GTest_FOUND)
        target_link_libraries(${target} PRIVATE GTest::gtest GTest::gtest_main)
    else()
        target_link_libraries(${target} PRIVATE gtest gtest_main)
    endif()
endmacro()

# ========== 测试可执行文件 ==========
add_executable(test_logger tests/test_logger.cpp src/logger/Logger.cpp)
target_link_libraries(test_logger PRIVATE ${SPDLOG_STATIC_LIB} ${FMT_STATIC_LIB})
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
    ${SPDLOG_STATIC_LIB}
    ${FMT_STATIC_LIB}
)
link_gtest(test_stdio_jsonrpc)

# http_jsonrpc_server 测试
add_executable(test_http_jsonrpc_server tests/test_http_jsonrpc_server.cpp
    src/json_rpc/jsonrpc_serialization.cpp
    src/json_rpc/jsonrpc.cpp
    src/json_rpc/http_jsonrpc_server.cpp
)
target_link_libraries(test_http_jsonrpc_server PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
)
link_gtest(test_http_jsonrpc_server)

# MCP模块测试
add_executable(test_mcp_server tests/test_mcp_server.cpp
    src/json_rpc/jsonrpc_serialization.cpp
    src/json_rpc/jsonrpc.cpp
    src/mcp/ToolManager.cpp
    src/mcp/ResourceManager.cpp
    src/mcp/PromptManager.cpp
    src/mcp/McpServer.cpp
    src/logger/Logger.cpp
)
target_link_libraries(test_mcp_server PRIVATE
    nlohmann_json::nlohmann_json
    ${SPDLOG_STATIC_LIB}
    ${FMT_STATIC_LIB}
)
link_gtest(test_mcp_server)

# ========== 启用测试 ==========
enable_testing()

# ========== 启用测试发现 ==========
include(GoogleTest)
gtest_discover_tests(test_logger)
gtest_discover_tests(test_jsonrpc)
gtest_discover_tests(test_stdio_jsonrpc)
gtest_discover_tests(test_http_jsonrpc_server)
gtest_discover_tests(test_mcp_server)

# ========== 为 IDE 设置 ==========
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "Configuration complete! Use 'make' to build the project.")
