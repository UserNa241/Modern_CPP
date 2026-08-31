cmake_minimum_required(VERSION 4.3)
project(Test)

set(CMAKE_CXX_STANDARD 23)

add_executable(Test main.cpp)
target_compile_options(Test PRIVATE
        -fsanitize=address
        -fno-omit-frame-pointer
)

target_link_options(Test PRIVATE
        -fsanitize=address
)

add_executable(StorageDuration storage_duration.cpp)
add_executable(padding padding.cpp)
add_executable(Lifetime object_lifetime.cpp)
add_executable(Inherit_Layout Inheritance_Layout.cpp)
add_executable(value_category value_categories.cpp)
add_executable(Temporary temporaries.cpp)
add_executable(Bit_Tools 1.7_bit_tools.cpp)
add_executable(Placement_new 1.8_placement_new.cpp)
add_executable(False_Sharing 1.10_false_sharing.cpp)
add_executable(Virtual_Memory 1.11_virtual_memory.cpp)
add_executable(RAII 2.1_RAII.cpp)