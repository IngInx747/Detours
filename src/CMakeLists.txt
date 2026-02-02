project(detours)

file(GLOB SRCS
  "detours.h"
  "detours.cpp"
  "modules.cpp"
  "disasm.cpp"
  "image.cpp"
  "creatwth.cpp"
  "disolx86.cpp"
  "disolx64.cpp"
  "disolia64.cpp"
  "disolarm.cpp"
)

add_library(${PROJECT_NAME} ${SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(FILES "detours.h" DESTINATION include/detours)