cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(promise)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
endif(MSVC)

file(GLOB test_sources "*.cpp" "*.hpp")
add_executable(${PROJECT_NAME} ${test_sources})
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

enable_testing()
add_test(${PROJECT_NAME} ${PROJECT_NAME})
