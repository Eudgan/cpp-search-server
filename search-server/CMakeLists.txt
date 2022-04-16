cmake_minimum_required(VERSION 3.20)

project(search_server_deque LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(search_server_deque
 main.cpp
 document.cpp
 read_input_functions.cpp
 request_queue.cpp
 search_server.cpp
 string_processing.cpp
 remove_duplicates.cpp
)
