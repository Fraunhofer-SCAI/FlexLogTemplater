cmake_minimum_required(VERSION 3.11)

project(logtools LANGUAGES CXX)

foreach(toolname IN ITEMS online_templater)
  add_executable(${toolname} ${toolname}.cpp)
  target_link_libraries(${toolname} PRIVATE fltlib)
endforeach(toolname)
