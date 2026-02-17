add_subdirectory(transport_interop)

add_library(test_plans_common
  common.cpp
)
target_link_libraries(test_plans_common
  hiredis::hiredis
)
