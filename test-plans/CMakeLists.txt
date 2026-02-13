add_subdirectory(ping)

add_library(test_plans_common
  common.cpp
)
target_link_libraries(test_plans_common
  hiredis::hiredis
)
