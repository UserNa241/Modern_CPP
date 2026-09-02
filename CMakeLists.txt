cmake_minimum_required(VERSION 4.4)
project(Test)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(/fsanitize=address /Zi)
add_link_options(/INCREMENTAL:NO)

## Global compiler configuration
#if(MSVC)
#    # Correct MSVC syntax for C++26 preview tools
#    add_compile_options(/std:c++latest)
#
#    # Global ASan flags using correct MSVC syntax
#    add_compile_options(/fsanitize=address /Zi)
#    add_link_options(/INCREMENTAL:NO)
#
#    # Remove conflicting Edit & Continue flags that CLion injecting by default
#    string(REPLACE "/ZI" "/Zi" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
#    string(REPLACE "/ZI" "/Zi" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
#else()
#    # Fallback for GCC/Clang (Linux/macOS)
#    set(CMAKE_CXX_STANDARD 26)
#    set(CMAKE_CXX_STANDARD_REQUIRED ON)
#    add_compile_options(-fsanitize=address -g)
#    add_link_options(-fsanitize=address)
#endif()

# Define your executables (ASan is now automatically inherited globally)
add_executable(Test main.cpp)
#target_compile_options(Test PRIVATE
#        -fsanitize=address
#        -g
#)
#target_link_options(Test PRIVATE
#        -fsanitize=address
#)

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
add_executable(RO5 2.2_Ruleof5.cpp)
add_executable(ExSaftey 2.3_ExceptSaftey.cpp)
