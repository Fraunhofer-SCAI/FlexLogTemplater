cmake_minimum_required(VERSION 3.11)

project(logtests LANGUAGES CXX)

foreach(testname IN ITEMS agglo cache_adp dist_classifier hc lcs lcs_complex
  levensh logps ordered_string_cache syslog_cluster syslog_cluster_by_tag
  syslog_nested_cluster_by_tag syslog_reader WED wit)
  
  add_executable(test_${testname} test_${testname}.cpp)
  target_link_libraries(test_${testname} PRIVATE fltlib)
endforeach(testname)
