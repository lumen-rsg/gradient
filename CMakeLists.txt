cmake_minimum_required(VERSION 3.15)
project(gradient CXX)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER clang)

# Dependencies
find_package(yaml-cpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
find_package(CURL REQUIRED)

# Include directories
include_directories(
        ${YAML_CPP_INCLUDE_DIRS}
        ${SQLITE3_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
        include
)

# Library search paths
link_directories(
        ${YAML_CPP_LIBRARY_DIRS}
        ${SQLITE3_LIBRARY_DIRS}
        ${CURL_LIBRARY_DIRS}
)

enable_testing()

add_executable(gradient
        src/main.cpp
        src/CLI.cpp
        src/Package.cpp
        src/Repository.cpp
        src/Database.cpp
        src/Installer.cpp
        src/TarHandler.cpp
        src/YamlParser.cpp
        src/ScriptExecutor.cpp
        src/DependencyResolver.cpp
        include/CLI.h
        include/cxxopts.h
        include/tools.h
        include/DownloadHelper.h
)

target_link_libraries(gradient
        yaml-cpp
        ${SQLITE3_LIBRARIES}
        ${CURL_LIBRARIES}
)

# install target
install(TARGETS gradient RUNTIME DESTINATION bin)