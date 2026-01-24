#ifndef LIB_H
#define LIB_H

#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <tinydir.h>
#include <tree_sitter/api.h>
#include <vector>

using namespace std;

void print_TSNode(const string source_code, const TSNode node,
                  const string &indent = "") {
  if (ts_node_is_null(node)) {
    return;
  }

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

void prettyPrintTSTree(const string &source, TSNode node,
                       const string &prefix = "", bool is_last = true) {
  if (ts_node_is_null(node))
    return;

  const char *type = ts_node_type(node);

  // Print tree branch
  cout << prefix;
  cout << (is_last ? "└── " : "├── ");
  cout << type;

  // Optional: show identifier / literal text
  uint32_t start = ts_node_start_byte(node);
  uint32_t end = ts_node_end_byte(node);

  if (strcmp(type, "identifier") == 0 || strcmp(type, "string_literal") == 0) {
    cout << ": \"" << source.substr(start, end - start) << "\"";
  }

  cout << endl;

  // Prepare prefix for children
  string child_prefix = prefix + (is_last ? "    " : "│   ");

  uint32_t child_count = ts_node_child_count(node);
  for (uint32_t i = 0; i < child_count; ++i) {
    TSNode child = ts_node_child(node, i);
    bool last_child = (i == child_count - 1);
    prettyPrintTSTree(source, child, child_prefix, last_child);
  }
}

void createDirectoriesForPath(const string &path) {
  size_t pos = 0;
  while ((pos = path.find_first_of("/\\", pos)) != string::npos) {
    string sub = path.substr(0, pos++);
#ifdef _WIN32
    _mkdir(sub.c_str());
#else
    mkdir(sub.c_str(), 0755);
#endif
  }
}

void collectNodesOfType(TSNode node, const string &type,
                        vector<TSNode> &result) {
  if (ts_node_is_null(node))
    return;

  const char *nodeType = ts_node_type(node);
  if (type == nodeType) {
    result.push_back(node);
  }

  uint32_t childCount = ts_node_child_count(node);
  for (uint32_t i = 0; i < childCount; ++i) {
    TSNode child = ts_node_child(node, i);
    collectNodesOfType(child, type, result);
  }
}

class File {
public:
  string source;
  tinydir_file tfile;
  TSTree *tree;

  File() {}

  File(string source, tinydir_file tfile, TSTree *tree) {
    this->source = source;
    this->tfile = tfile;
    this->tree = tree;
  }

  void printContent() {
    string header =
        "==============" + string(this->tfile.path) + "=======================";
    cout << header << endl;
    cout << this->source << endl;
    cout << string(header.length(), '=') << endl;
  }

  void print() {
    this->printContent();
    prettyPrintTSTree(this->source, ts_tree_root_node(this->tree));
  }

  ~File() { ts_tree_delete(tree); }
};

class ReplaceEngine {

private:
  const TSLanguage *lang;
  TSParser *parser;

public:
  ReplaceEngine(const TSLanguage *lang) {
    this->lang = lang;
    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, lang);
    this->parser = parser;
  }

  TSTree *getTSTreeFromSource(string source) {
    TSTree *tree = ts_parser_parse_string(this->parser, NULL, source.c_str(),
                                          strlen(source.c_str()));
    return tree;
  }

  File readFile(const string path) {
    File file;

    if (tinydir_file_open(&file.tfile, path.c_str()) == -1) {
      cerr << "Error opening file: " << path << endl;
      return file;
    }

    ifstream in(path, ios::binary);
    if (!in) {
      cerr << "Failed to read file: " << path << endl;
      return file;
    }

    in.seekg(0, ios::end);
    size_t size = in.tellg();
    string buffer(size, ' ');
    in.seekg(0);
    in.read(&buffer[0], size);

    file.source = buffer;
    file.tree = this->getTSTreeFromSource(buffer);
    return file;
  }

  ~ReplaceEngine() { ts_parser_delete(parser); }
};

/**
 * 1. Operation
 * 2. Worker who will do all these operations on files
 * 3. Planner who will spin up the workers and wait on them,
 * 4.
 */

class Operation {
public:
  virtual ~Operation() {}
  virtual void apply(File &file) = 0;
};

class ReplaceOpr : public Operation {
public:
  enum Type { FIRST, LAST, ALL, REX };

private:
  string from;
  string to;
  regex fromRe;
  Type type;
  string *replace(string *buf, size_t pos, string to, size_t resize_by) {
    if (resize_by != 0) {
      buf->resize(buf->size() + resize_by);
    }

    memcpy(&(buf[pos]), to.c_str(), to.size());

    return buf;
  }

public:
  ReplaceOpr(string from, string to, ReplaceOpr::Type type = ALL) {
    this->from = from;
    this->to = to;
    this->type = type;
    if (type == REX) {
      try {
        this->fromRe = regex(from);
      } catch (const std::regex_error &e) {
        cerr << "Regex error: " << e.what() << endl;
        this->type = FIRST;
      }
    }
  }

  void apply(File &file) override {
    string &buf = file.source;

    if (type == FIRST) {
      size_t pos = buf.find(from);
      if (pos != string::npos)
        buf.replace(pos, from.size(), to);
      return;
    }

    if (type == LAST) {
      size_t pos = buf.rfind(from);
      if (pos != string::npos)
        buf.replace(pos, from.size(), to);
      return;
    }

    if (type == ALL) {
      size_t oldContentSize = from.size();
      size_t newContentSize = to.size();

      size_t lastOccurence = 0;
      size_t pos = 0;
      size_t buffResizeBy = (-oldContentSize + newContentSize);

      while (pos < buf.size()) {
        if (buf.substr(pos, oldContentSize) == from) {
          this->replace(&buf, pos, this->to, buffResizeBy);
          pos += newContentSize;
        } else {
          pos++;
        }
      }
      return;
    }

    if (type == REX) {
      try {
        buf = regex_replace(buf, fromRe, to);
      } catch (const std::regex_error &e) {
        cerr << "Regex error: " << e.what() << endl;
      }
      return;
    }
  }
};

class ReplaceASTAwareOpr : public Operation {
  // https://github.com/tree-sitter/tree-sitter/blob/master/docs/src/using-parsers/6-static-node-types.md
private:
  string ifOfType;
  string from;
  string to;
  string queryPred;

public:
  ReplaceASTAwareOpr(string from, string to, string ifOfType,
                  string queryPred = "") {
    this->from = from;
    this->to = to;
    this->ifOfType = ifOfType;
    this->queryPred = queryPred;
  }

  void apply(File &file) {
    if (ifOfType.empty() && queryPred.empty()) {
      ReplaceOpr replace(this->from, this->to);
      replace.apply(file);
      return;
    }
    vector<TSNode> nodes;
    collectNodesOfType(ts_tree_root_node(file.tree), ifOfType, nodes);
    for(auto node: nodes){
      print_TSNode(file.source, node);
    }
  }
};

class SaveOpr : public Operation {
public:
  string filePrefix;
  string fileSuffix;
  string pathOffset;
  SaveOpr(string folderPathOffset = "", string filePrefix = "",
          string fileSuffix = "") {
    this->filePrefix = filePrefix;
    this->fileSuffix = fileSuffix;
    this->pathOffset = folderPathOffset;
  }

  void apply(File &file) override {
    if (file.tfile.is_dir)
      return;

    string originalPath = file.tfile.path;

    // -----------------------------
    // Split directory + filename
    // -----------------------------
    size_t lastSlash = originalPath.find_last_of("/\\");
    string dir = (lastSlash == string::npos)
                     ? ""
                     : originalPath.substr(0, lastSlash + 1);
    string filename = (lastSlash == string::npos)
                          ? originalPath
                          : originalPath.substr(lastSlash + 1);

    // -----------------------------
    // Split filename + extension
    // -----------------------------
    size_t lastDot = filename.find_last_of('.');
    string name =
        (lastDot == string::npos) ? filename : filename.substr(0, lastDot);
    string ext = (lastDot == string::npos) ? "" : filename.substr(lastDot);

    // -----------------------------
    // Apply prefix / suffix
    // -----------------------------
    string newFilename = filePrefix + name + fileSuffix + ext;

    // -----------------------------
    // Apply path offset
    // -----------------------------
    string outputPath;
    if (!pathOffset.empty()) {
      outputPath = pathOffset;

      // ensure trailing slash
      if (outputPath.back() != '/' && outputPath.back() != '\\')
        outputPath += '/';

      outputPath += dir + newFilename;
    } else {
      outputPath = dir + newFilename;
    }

    // -----------------------------
    // Ensure directories exist
    // -----------------------------
    createDirectoriesForPath(outputPath);

    // -----------------------------
    // Save file
    // -----------------------------
    ofstream out(outputPath, ios::binary);
    out << file.source;
    out.close();
  }
};

#endif
