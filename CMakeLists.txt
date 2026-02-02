cmake_minimum_required(VERSION 3.0)

project(DETOURS VERSION 1.0 LANGUAGES CXX)

add_subdirectory(src)

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  add_subdirectory(samples)
endif()