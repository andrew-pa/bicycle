cmake_minimum_required(VERSION 3.16)

project(bicycle VERSION 1.0 LANGUAGES CXX)

add_library(bicycle_common
    src/eval.cpp src/parser.cpp src/tokenizer.cpp src/intrp_std.cpp
    inc/ast.h inc/eval.h inc/parse.h inc/token.h inc/intrp_std.h)
target_include_directories(bicycle_common PUBLIC inc/)
target_compile_features(bicycle_common PUBLIC cxx_std_17)

add_executable(bicycle_src_intrp src/src_intrp.cpp)
target_include_directories(bicycle_src_intrp PUBLIC inc/)
target_link_libraries(bicycle_src_intrp PRIVATE bicycle_common)
target_compile_features(bicycle_src_intrp PUBLIC cxx_std_17)

add_executable(bicycle_vmi src/vm_intrp.cpp)
target_include_directories(bicycle_vmi PUBLIC inc/)
target_link_libraries(bicycle_vmi PRIVATE bicycle_common)
target_compile_features(bicycle_vmi PUBLIC cxx_std_17)
