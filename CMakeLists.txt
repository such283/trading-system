cmake_minimum_required(VERSION 3.30)
project(trading_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)

add_executable(trading
        main.cpp
        src/Authentication.cpp
        include/websocket_server.hpp
        src/websocket_server.cpp
        include/market_data.hpp
        src/market_data.cpp
        include/deribit_client.hpp
        src/deribit_client.cpp
)

# Tell WebSocket++ to use standalone Asio
target_compile_definitions(trading PRIVATE
        ASIO_STANDALONE
        _WEBSOCKETPP_CPP11_STL_
)

# Add your downloaded Asio headers
target_include_directories(trading PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        /Users/chitumalla/Downloads/asio-1.18.0/include
)

# Link required libraries
target_link_libraries(trading PRIVATE
        cpprestsdk::cpprest
        OpenSSL::SSL
        OpenSSL::Crypto
        jsoncpp_lib
)
