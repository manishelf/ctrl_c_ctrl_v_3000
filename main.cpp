#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tinydir.h>
#include <tree_sitter/api.h>

using namespace std;

extern "C" {
const TSLanguage *tree_sitter_java(void);
}

void printFileContent(const string path) {
  ifstream file("sample.txt");
  if (!file.is_open()) {
    cerr << "File " << path << " could not be opened!" << endl;
  }
  string str;
  string header = "==============" + path + "=======================";
  cout << header << endl;
  while (getline(file, str)) {
    cout << str << endl;
  }
  cout << string(header.length(), '=') << endl;
}

void printDirectoryContent(const string path, bool recursive) {
  static int indent = 0;
  tinydir_dir dir;
  int i;
  if (tinydir_open_sorted(&dir, path.c_str()) != 0) {
    cerr << "Failed to open dir " << path << endl;
    return;
  }

  // string header = "->" + path;
  // cout << endl << string(indent, ' ') << header;
  cout << endl;
  for (i = 0; i < dir.n_files; i++) {
    tinydir_file file;
    tinydir_readfile_n(&dir, &file, i);

    if (strcmp(file.name, ".") == 0 || strcmp(file.name, "..") == 0)
      continue;

    cout << string(indent, ' ') << file.name;
    if (file.is_dir) {
      cout << "/";
      if (recursive) {
        string subDir = path;
        if (path.back() != '/') {
          subDir = path + "/";
        }
        subDir += file.name;
        indent += 2;
        printDirectoryContent(subDir, recursive);
        indent -= 2;
      }
    }
    cout << endl;
  }
  tinydir_close(&dir);
}

void print_TSNode(const string source_code, const TSNode node,
                  const string &indent = "") {
  const char *node_type = ts_node_type(node);
  // Get the start and end byte positions of the node
  uint32_t start_byte = ts_node_start_byte(node);
  uint32_t end_byte = ts_node_end_byte(node);

  // Extract the text of the node (source code corresponding to this node)
  string node_text = source_code.substr(start_byte, end_byte - start_byte);

  cout << indent << node_type << ": \"" << node_text << "\"" << endl;

  int child_count = ts_node_child_count(node);
  for (int i = 0; i < child_count; ++i) {
    TSNode child = ts_node_child(node, i);
    print_TSNode(source_code, child, indent + "  ");
  }
}

void parseAndPrintFile(const string path, const TSLanguage *lang) {

  TSParser *parser = ts_parser_new();

  ts_parser_set_language(parser, lang);

  ifstream file(path);

  if (!file.is_open()) {
    cerr << "file not opened!";
  }

  stringstream buff;

  buff << file.rdbuf();

  const string &source_code = buff.str();

  cout << "Source" << endl;
  cout << source_code << endl;

  TSTree *tree = ts_parser_parse_string(parser, NULL, source_code.c_str(),
                                        strlen(source_code.c_str()));

  TSNode root_node = ts_tree_root_node(tree);

  TSNode array_node = ts_node_named_child(root_node, 0);
  TSNode number_node = ts_node_named_child(array_node, 0);

  // Print the syntax tree as an S-expression.

  string header = "==============" + path + "=======================";
  cout << header << endl;
  print_TSNode(source_code, root_node);
  cout << string(header.length(), '=') << endl;

  // Free all of the heap-allocated memory.
  ts_tree_delete(tree);
  ts_parser_delete(parser);
}

int main(int argc, char *argv[]) {

  cout << "Hello world" << endl;

  /**
   * 1. read a file and parse from tree-sitter
   * 2. threads
   */
  parseAndPrintFile("sample.java", tree_sitter_java());
  // printFileContent("sample.txt");
  // printDirectoryContent("./tree-sitter/lib", true);
  return 0;
}
