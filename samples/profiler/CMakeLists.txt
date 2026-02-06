project(profiler)

add_executable(profiler "profiler.cpp")
target_link_libraries(profiler detours)
add_dependencies     (profiler detours)

add_executable(profiler_target "target.cpp")

add_library(profiler_payload SHARED "payload.cpp")
target_link_libraries(profiler_payload detours)
add_dependencies     (profiler_payload detours)
set_target_properties(profiler_payload PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)