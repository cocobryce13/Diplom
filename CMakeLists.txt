cmake_minimum_required(VERSION 3.10)
project(DiplomProject)

# Устанавливаем стандарт C++20 вместо C++17
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск CURL
find_package(CURL REQUIRED)

# Ручной поиск libpqxx
find_path(PQXX_INCLUDE_DIR
    NAMES pqxx/pqxx
    PATHS
        "C:/Users/sokol/vcpkg/installed/x64-windows/include"
    REQUIRED
)

find_library(PQXX_LIBRARY
    NAMES pqxx
    PATHS
        "C:/Users/sokol/vcpkg/installed/x64-windows/lib"
    REQUIRED
)

find_library(PQ_LIBRARY
    NAMES libpq pq
    PATHS
        "C:/Users/sokol/vcpkg/installed/x64-windows/lib"
    REQUIRED
)

message(STATUS "PQXX include: ${PQXX_INCLUDE_DIR}")
message(STATUS "PQXX library: ${PQXX_LIBRARY}")
message(STATUS "PQ library: ${PQ_LIBRARY}")

include_directories(
    ${CURL_INCLUDE_DIRS}
    ${PQXX_INCLUDE_DIR}
    include
)

add_executable(crawler 
    src/main_crawler.cpp 
    src/crawler_utils.cpp 
    src/config.cpp 
    src/indexer.cpp
)

target_link_libraries(crawler 
    ${CURL_LIBRARIES}
    ${PQXX_LIBRARY}
    ${PQ_LIBRARY}
)

add_executable(server 
    src/server.cpp 
    src/server_utils.cpp 
    src/config.cpp
)

# Для сервера добавляем Winsock библиотеку
target_link_libraries(server 
    ${CURL_LIBRARIES}
    ${PQXX_LIBRARY}
    ${PQ_LIBRARY}
    ws2_32
)