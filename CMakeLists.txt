cmake_minimum_required(VERSION 3.10)
project(DiplomProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 и выше
endif()

find_package(Boost REQUIRED COMPONENTS beast system)

find_path(PQXX_INCLUDE_DIR
    NAMES pqxx/pqxx
    HINTS
        "C:/Users/sokol/vcpkg/installed/x64-windows/include"
    REQUIRED
)

find_library(PQXX_LIBRARY
    NAMES pqxx
    HINTS  
        "C:/Users/sokol/vcpkg/installed/x64-windows/lib"
    REQUIRED
)

find_library(PQ_LIBRARY
    NAMES libpq
    HINTS
        "C:/Users/sokol/vcpkg/installed/x64-windows/lib"
    REQUIRED
)

message(STATUS "Boost found: ${Boost_VERSION}")
message(STATUS "PQXX include: ${PQXX_INCLUDE_DIR}")
message(STATUS "PQXX library: ${PQXX_LIBRARY}")
message(STATUS "PQ library: ${PQ_LIBRARY}")

include_directories(
    ${Boost_INCLUDE_DIRS}
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
    Boost::beast
    Boost::system
    ${PQXX_LIBRARY}
    ${PQ_LIBRARY}
)

add_executable(server 
    src/server.cpp 
    src/server_utils.cpp 
    src/config.cpp
)

target_link_libraries(server 
    ${PQXX_LIBRARY}
    ${PQ_LIBRARY}
    ws2_32
)