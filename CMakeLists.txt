cmake_minimum_required(VERSION 3.15)
project(htserve C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(htserve src/htserve.c)

if(WIN32)
  target_link_libraries(htserve PRIVATE ws2_32)
  target_link_options(htserve PRIVATE -static -static-libgcc)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(htserve PRIVATE Threads::Threads)
endif()

target_compile_options(htserve PRIVATE -Wall -Wextra -O2)
