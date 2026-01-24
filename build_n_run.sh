#!/usr/bin/env bash

set -e

rm -f ctrl_c_ctrl_v_3000

echo "=========================="
echo "make tree-sitter and tree-sitter-java"
make tree-sitter
make tree-sitter-java
echo "building tree-sitter"
gcc \
  -I tree-sitter/lib/include       \
  -I tree-sitter-java/src          \
  -c tree-sitter-java/src/parser.c \
  -o tree_sitter_java.o

echo "building main"
g++ \
  -I ./include                    \
  -I tree-sitter/lib/include      \
  -I tree-sitter-java/src         \
  -I ./tinydir                    \
  -c main.cpp                     \
  -o ctrl_c_ctrl_v_3000.o

echo "linking main to tree-sitter"
g++ \
  ctrl_c_ctrl_v_3000.o            \
  tree_sitter_java.o              \
  tree-sitter/libtree-sitter.a    \
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
