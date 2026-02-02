project(member)
add_executable(${PROJECT_NAME} "member.x64.cpp")
target_link_libraries(${PROJECT_NAME} detours)
add_dependencies     (${PROJECT_NAME} detours)