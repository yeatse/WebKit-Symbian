cmake_minimum_required(VERSION 3.28)
project(sqlite_probe LANGUAGES CXX ASM)
include(SymbianApp)

set(SQLITE_DEPENDENCY_PREFIX "" CACHE PATH "The build-deps.sh install directory used by the browser")
if(NOT EXISTS "${SQLITE_DEPENDENCY_PREFIX}/include/sqlite3.h" OR NOT EXISTS "${SQLITE_DEPENDENCY_PREFIX}/lib/libsqlite3.a")
    message(FATAL_ERROR "Set SQLITE_DEPENDENCY_PREFIX to the browser's deps/install directory")
endif()

symbian_add_executable(sqlite_probe sqlite-probe.cpp)
target_include_directories(sqlite_probe PRIVATE "${SQLITE_DEPENDENCY_PREFIX}/include")
target_link_libraries(sqlite_probe PRIVATE "${SQLITE_DEPENDENCY_PREFIX}/lib/libsqlite3.a" Symbian::Pthread Symbian::Math)
# Use the browser's capability mask; this probe needs no additional privileges.
symbian_publish_executable(sqlite_probe UID3 0xe00008f1 CAPABILITIES 0x2000)
