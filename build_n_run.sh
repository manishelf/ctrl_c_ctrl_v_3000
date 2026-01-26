#!/usr/bin/env bash

set -e

rm -f ctrl_c_ctrl_v_3000

echo "=========================="
echo "make tree-sitter and tree-sitter-java"
make ./deps/tree-sitter
make ./deps/tree-sitter-parsers/tree-sitter-java
echo "building tree-sitter"
gcc \
  -I ./deps/tree-sitter/lib/include                           \
  -I ./deps/tree-sitter-java/src                              \
  -c ./deps/tree-sitter-parsers/tree-sitter-java/src/parser.c \
  -o tree_sitter_java.o

echo "building main"
g++ \
  -I ./include                                               \
  -I ./deps/tree-sitter/lib/include                          \
  -I ./deps/tree-sitter-parsers/tree-sitter-java/src         \
  -I ./deps/tinydir                                          \
  -c main.cpp                                                \
  -o ctrl_c_ctrl_v_3000.o

echo "linking main to tree-sitter"
g++ \
  ctrl_c_ctrl_v_3000.o                    \
  tree_sitter_java.o                      \
  ./deps/tree-sitter/libtree-sitter.a     \
  -o ctrl_c_ctrl_v_3000

echo "built main executable"
echo "=========================="

#g++                             \
#  -I tree-sitter/lib/include    \
#  -I ./tinydir                  \
#  main.cpp                      \
#  tree-sitter-java/src/parser.c \
#  tree-sitter/libtree-sitter.a  \
#  -o main


./ctrl_c_ctrl_v_3000
