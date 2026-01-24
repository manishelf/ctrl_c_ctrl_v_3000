cmake_minimum_required(VERSION 3.10)

project(CtrlC_CtrlV_3000 VERSION 1.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include directories
include_directories(
    ./include
    ./tinydir
    tree-sitter/lib/include
    tree-sitter-java/src
)

# Add tree-sitter as a subdirectory and build it if not already done
add_subdirectory(tree-sitter)

# Build tree-sitter-java's parser.c as an object
add_library(tree_sitter_java_obj OBJECT tree-sitter-java/src/parser.c)

# List source files for the main project
set(SOURCES
    main.cpp
    $<TARGET_OBJECTS:tree_sitter_java_obj>  # Add tree_sitter_java_obj as an object
)

# Create the main executable
add_executable(ctrl_c_ctrl_v_3000 ${SOURCES})

# Link tree-sitter static library
target_link_libraries(ctrl_c_ctrl_v_3000 PRIVATE
    ${CMAKE_SOURCE_DIR}/tree-sitter/libtree-sitter.a  # Path to static lib
)

# Optional: Enable debug symbols and warnings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ctrl_c_ctrl_v_3000 PRIVATE -g -Wall)
endif()

# Add custom command to ensure tree-sitter is built before main executable
add_dependencies(ctrl_c_ctrl_v_3000 tree-sitter)
