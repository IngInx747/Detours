project(simple)

add_executable(sleep5 "sleep5.cpp")

add_library(simple SHARED "simple.cpp")
target_link_libraries(simple detours)
add_dependencies     (simple detours)
set_target_properties(simple PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)