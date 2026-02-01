# Compiler settings
CC      := gcc
CXX     := g++
CFLAGS  := -I deps/tree-sitter/lib/include \
           -I deps/tree-sitter-parsers/tree-sitter-java/src \
					 -g -O0

CXXFLAGS := -I ./include \
            -I deps/tree-sitter/lib/include \
            -I deps/tree-sitter/parsers/tree-sitter-java/src \
            -I deps/tinydir \
						-g -O0

LDFLAGS := deps/tree-sitter/libtree-sitter.a

# Files
TARGET  := ctrl_c_ctrl_v_3000
OBJ     := ctrl_c_ctrl_v_3000.o tree_sitter_java.o

# Default target
all: deps/tree-sitter deps/tree-sitter-parsers/tree-sitter-java $(TARGET)

# Build tree-sitter
tree-sitter:
	$(MAKE) -g -O0 -C deps/tree-sitter

# Build tree-sitter-java
tree-sitter-java:
	$(MAKE) -g -O0 -C deps/tree-sitter-parsers/tree-sitter-java

# Compile Java parser
tree_sitter_java.o: deps/tree-sitter-parsers/tree-sitter-java/src/parser.c
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
