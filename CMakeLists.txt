cmake_minimum_required(VERSION 3.15)
project(gdb C)

set(CMAKE_C_STANDARD 99)

# Define source file(s)
add_executable(gdb
    src/main.c
)

# Include path for OpenCL headers
target_include_directories(gdb PRIVATE
    ${CMAKE_SOURCE_DIR}/src/third_party/CL/
)

# Link the local OpenCL static library
target_link_libraries(gdb PRIVATE
    ${CMAKE_SOURCE_DIR}/src/third_party/CL/OpenCL.lib
)