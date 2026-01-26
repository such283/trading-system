cmake_minimum_required(VERSION 3.15)
project(trading_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)

# Auto-download Asio 1.18.0 if not found
set(ASIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/asio/include)

if(NOT EXISTS ${ASIO_INCLUDE_DIR}/asio.hpp)
    message(STATUS "Asio not found. Downloading Asio 1.18.0...")

    file(DOWNLOAD
            https://sourceforge.net/projects/asio/files/asio/1.18.0%20%28Stable%29/asio-1.18.0.tar.gz/download
            ${CMAKE_BINARY_DIR}/asio-1.18.0.tar.gz
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
    )

    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download Asio 1.18.0")
    endif()

    message(STATUS "Extracting Asio 1.18.0...")
    execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_BINARY_DIR}/asio-1.18.0.tar.gz
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/external/asio)
    file(COPY ${CMAKE_BINARY_DIR}/asio-1.18.0/include/
            DESTINATION ${ASIO_INCLUDE_DIR})

    message(STATUS "Asio 1.18.0 installed to ${ASIO_INCLUDE_DIR}")
endif()

message(STATUS "Using Asio: ${ASIO_INCLUDE_DIR}")

add_executable(trading
        main.cpp
        src/Authentication.cpp
        src/market_data.cpp
        src/deribit_client.cpp
        src/order.cpp
)

# Tell WebSocket++ to use standalone Asio
target_compile_definitions(trading PRIVATE
        ASIO_STANDALONE
        _WEBSOCKETPP_CPP11_STL_
        CPPREST_FORCE_HTTP_CLIENT_ASIO
        CPPREST_FORCE_HTTP_LISTENER_ASIO
)

# Include directories
target_include_directories(trading PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${ASIO_INCLUDE_DIR}
)

# Link required libraries
target_link_libraries(trading PRIVATE
        cpprestsdk::cpprest
        OpenSSL::SSL
        OpenSSL::Crypto
        jsoncpp_lib
)