cmake_minimum_required(VERSION 3.15)
project(anemo CXX)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER clang)

# Dependencies
find_package(yaml-cpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# CLI11 or cxxopts (header-only) must be provided in include/
include_directories(${YAML_CPP_INCLUDE_DIRS} ${SQLITE3_INCLUDE_DIRS} include)
link_directories(${YAML_CPP_LIBRARY_DIRS} ${SQLITE3_LIBRARY_DIRS})

enable_testing()

add_executable(anemo
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
        src/CLI.cpp
        include/cxxopts.h
)

target_link_libraries(anemo yaml-cpp ${SQLITE3_LIBRARIES})

# install target
install(TARGETS anemo RUNTIME DESTINATION bin)