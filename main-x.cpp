#include <iostream>
#include <lib-x.h>
#include <lib.h>
#include <tree_sitter/api.h>
#include <ts_queries.h>
#include <vector>
#include <chrono>

using namespace std;

extern "C" {
const TSLanguage *tree_sitter_java(void);
}

class ParsedTSTree {
public:
  TSTree *tree;
  string source;

  ParsedTSTree(string source, TSTree *tree) {
    this->tree = tree;
    this->source = source;
  }

  ~ParsedTSTree() { ts_tree_delete(tree); }
};

class FileContentParser {
public:
  const TSLanguage *lang;
  TSParser *tsParser;
  TSTree *tsTree;
  string content;

  FileContentParser(const TSLanguage *lang) {
    this->lang = lang;
    this->tsParser = ts_parser_new();
    ts_parser_set_language(this->tsParser, lang);
  };

  ParsedTSTree parseTree(string source) {
    TSTree *tree = ts_parser_parse_string(this->tsParser, NULL, source.c_str(),
                                          source.length());
    return ParsedTSTree(source, tree);
  };

 vector<TSNode> walkTree(ParsedTSTree* tree,
                string filterQuery = "") {
    uint32_t errorOffset;
    TSQueryError error;
    vector<TSNode> result;

    TSQuery *query = ts_query_new(this->lang, filterQuery.c_str(),
                                  filterQuery.length(), &errorOffset, &error);

    TSQueryCursor *cursor = ts_query_cursor_new();

    TSNode root = ts_tree_root_node(tree->tree);

    ts_query_cursor_exec(cursor, query, root);
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
      for (uint32_t i = 0; i < match.capture_count; i++) {
        TSNode node = match.captures[i].node;
        if (ts_node_is_null(node)) continue;
        result.push_back(node);
      }
    }
    ts_query_delete(query);
    ts_query_cursor_delete(cursor);
    return result;
  };

  string nodeText(ParsedTSTree* tree, const TSNode &node) const {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    return tree->source.substr(start, end - start);
  }

  ~FileContentParser() { ts_parser_delete(this->tsParser); };
};

FileContentParser parser(tree_sitter_java());
size_t noOfMethods = 0; 
struct FileContentWalker {
  size_t nPreceding;
  size_t nFollowing;
  ENTRY_ACTION operator()(WALK_ENTRY_STATE walkState, TargetEntry *entry) {

    if (entry->name.compare("resources") == 0)
      return ENTRY_ACTION::SKIP;

    if (entry->isFile) {
      if (strcmp(entry->file.extension, "java") != 0)
        return ENTRY_ACTION::CONTINUE;
      cout << "READING - "<<entry->name <<endl;
      TargetFile *file = dynamic_cast<TargetFile *>(entry);
      if (file && file->loadFile()) {
        while (file->hasNextBlock) {
          TargetFile::Block block = file->next();
          string cont(block.data, block.size);
          
          ParsedTSTree tree = parser.parseTree(cont);
          
//          cout<<cont<<endl;
          vector methods = parser.walkTree(&tree, ts::java::queries::CONSTRUCTORS);
          noOfMethods+=methods.size();
        }
      }
    }

    return ENTRY_ACTION::CONTINUE;
  }
};

int main(int argc, char *argv[]) {

  cout << "Hello world" << endl;
  auto start = chrono::steady_clock::now();
  TargetDir dir(argv[1]);
  cout << dir.name << endl;

  FileContentWalker fileWalker;
  cout << dir.walk(true, fileWalker) << endl;
  auto end = chrono::steady_clock::now();
  cout << "done! - " << noOfMethods <<endl;
  auto duration = chrono::duration_cast<chrono::milliseconds>(end-start);
  cout << "in " << duration.count() <<" ms"<<endl;
  return 0;
}
