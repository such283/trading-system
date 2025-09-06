cmake_minimum_required(VERSION 3.30)
project(trading_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(trading main.cpp
        src/Authentication.cpp
)

target_include_directories(trading PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(trading PRIVATE
        cpprestsdk::cpprest
        OpenSSL::SSL
        OpenSSL::Crypto
)