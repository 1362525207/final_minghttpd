
cmake_minimum_required( VERSION 2.8 )

project( minghttpd )

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_CXX_FLAGS "-std=c++0x") 

link_libraries(pthread)

add_executable( httpserver mainfunc.cpp httpd.cpp)

