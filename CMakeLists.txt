cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(promise)

find_package(Threads REQUIRED)

file(GLOB test_sources "*.cpp" "*.hpp")
add_executable(${PROJECT_NAME} ${test_sources})

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif(MSVC)

target_link_libraries(${PROJECT_NAME}
    Threads::Threads)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

enable_testing()
add_test(${PROJECT_NAME} ${PROJECT_NAME})
