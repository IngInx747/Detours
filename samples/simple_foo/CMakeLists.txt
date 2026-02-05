project(simple_foo)

add_library(foo_lib SHARED "foo.cpp" "foo.h")

add_executable(foo_target "target.cpp")
target_link_libraries(foo_target foo_lib)
add_dependencies     (foo_target foo_lib)

add_library(foo_payload SHARED "payload.cpp")
target_link_libraries(foo_payload foo_lib)
add_dependencies     (foo_payload foo_lib)
target_link_libraries(foo_payload detours)
add_dependencies     (foo_payload detours)
set_target_properties(foo_payload PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)