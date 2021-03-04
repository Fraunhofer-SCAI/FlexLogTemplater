cmake_minimum_required(VERSION 3.11)

project(flexlogtemplater LANGUAGES CXX)

find_package(Threads REQUIRED)

add_library(fltlib INTERFACE)
target_link_libraries(fltlib INTERFACE Threads::Threads)
target_compile_features(fltlib INTERFACE cxx_std_17)
target_include_directories(fltlib INTERFACE .)
if(WIN32)
  target_link_libraries(fltlib INTERFACE ws2_32 ntdll)
  target_compile_definitions(fltlib INTERFACE _WIN32_WINNT=0x0A00)
endif()

add_subdirectory(tests)
add_subdirectory(tools)