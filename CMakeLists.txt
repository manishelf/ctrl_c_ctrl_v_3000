cmake_minimum_required(VERSION 3.10)

project(CtrlC_CtrlV_3000 VERSION 1.0 LANGUAGES C CXX)

# -----------------------
# C++ settings
# -----------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------
# Include directories
# -----------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/deps/tinydir
    ${CMAKE_SOURCE_DIR}/deps/tree-sitter/lib/include
    ${CMAKE_SOURCE_DIR}/deps/tree-sitter-parsers/tree-sitter-java/src
)

# -----------------------
# Build tree-sitter using Makefile
# -----------------------
add_custom_target(make_tree_sitter
    COMMAND make
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/tree-sitter
    BYPRODUCTS ${CMAKE_SOURCE_DIR}/deps/tree-sitter/libtree-sitter.a
)

# Tell CMake that libtree-sitter.a is a real library target
add_library(tree_sitter STATIC IMPORTED GLOBAL)

set_target_properties(tree_sitter PROPERTIES
    IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/deps/tree-sitter/libtree-sitter.a
)

# Ensure make runs before linking
add_dependencies(tree_sitter make_tree_sitter)

# -----------------------
# tree-sitter-java parser
# -----------------------
add_library(tree_sitter_java_obj OBJECT
    deps/tree-sitter-parsers/tree-sitter-java/src/parser.c
)

# -----------------------
# Main executable
# -----------------------
add_executable(ctrl_c_ctrl_v_3000
    main.cpp
    $<TARGET_OBJECTS:tree_sitter_java_obj>
)

# -----------------------
# Link libraries
# -----------------------
target_link_libraries(ctrl_c_ctrl_v_3000 PRIVATE
    tree_sitter
)

# -----------------------
# Debug flags
# -----------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ctrl_c_ctrl_v_3000 PRIVATE -g -Wall)
endif()
