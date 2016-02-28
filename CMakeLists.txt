project(final)

add_executable(final main.cpp)

set(CMAKE_CXX_FLAGS "-std=c++11")

target_link_libraries(final -pthread)
