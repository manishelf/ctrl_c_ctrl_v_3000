# Compiler settings
CC      := gcc
CXX     := g++
CFLAGS  := -I tree-sitter/lib/include \
           -I tree-sitter-java/src

CXXFLAGS := -I ./include \
            -I tree-sitter/lib/include \
            -I tree-sitter-java/src \
            -I ./tinydir

LDFLAGS := tree-sitter/libtree-sitter.a

# Files
TARGET  := ctrl_c_ctrl_v_3000
OBJ     := ctrl_c_ctrl_v_3000.o tree_sitter_java.o

# Default target
all: tree-sitter tree-sitter-java $(TARGET)

# Build tree-sitter
tree-sitter:
	$(MAKE) -C tree-sitter

# Build tree-sitter-java
tree-sitter-java:
	$(MAKE) -C tree-sitter-java

# Compile Java parser
tree_sitter_java.o: tree-sitter-java/src/parser.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile main
ctrl_c_ctrl_v_3000.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJ)
	$(CXX) $(OBJ) $(LDFLAGS) -o $@

# Run
run: $(TARGET)
	./$(TARGET)

# Clean
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean run tree-sitter tree-sitter-java
