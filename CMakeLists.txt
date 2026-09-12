cmake_minimum_required(VERSION 4.4)
project(Test)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(/fsanitize=address /Zi)
add_link_options(/INCREMENTAL:NO)
add_compile_options(/EHsc)

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

add_executable(test_pool test_pool_alloc.cpp)

add_executable(1.01_storage_duration 1.1_storage_duration.cpp)
add_executable(1.02_padding 1.2_padding.cpp)
add_executable(1.03_lifetime 1.3_object_lifetime.cpp)
add_executable(1.04_inherit_layout 1.4_inheritance_layout.cpp)
add_executable(1.05_value_category 1.5_value_categories.cpp)
add_executable(1.06_temporary 1.6_temporaries.cpp)
add_executable(1.07_bit_tools 1.7_bit_tools.cpp)
add_executable(1.08_placement_new 1.8_placement_new.cpp)
add_executable(1.09_false_sharing 1.9_false_sharing.cpp)
add_executable(1.10_virtual_memory 1.10_virtual_memory.cpp)
add_executable(2.01_RAII 2.1_RAII.cpp)
add_executable(2.02_ro5 2.2_rule_of_5.cpp)
add_executable(2.03_exception_saftey 2.3_exception_saftey.cpp)
add_executable(2.04_handle 2.4_handle.cpp)
add_executable(2.05_mapped_region 2.5_mapped_region.cpp)
add_executable(2.06_scopeguard 2.6_scope_guard.cpp)
add_executable(2.07_mutexwrapper 2.7_mutex_wrapper.cpp)
add_executable(2.08_RAIIxmove 2.8_RAIIxmove.cpp)
add_executable(2.09_partial_const 2.9_partial_construction.cpp)
add_executable(3.01_unique_ptr 3.1_unique_ptr.cpp)
add_executable(3.02_shared_ptr 3.2_shared_ptr.cpp)