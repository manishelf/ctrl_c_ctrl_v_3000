#include <lib.h>
#include<vector>
#include <tree_sitter/api.h>

using namespace std;

extern "C" {
const TSLanguage *tree_sitter_java(void);
}

DirWalker::ACTION printActionSync(DirWalker::STATUS status, File file, void* payload) {

  cout << file.name <<endl;

 if (status == DirWalker::STATUS::OPENED && !file.isDir) {
    if(file.ext == "java"){ 
      FileReader reader(file);
      TSTree *tree = ts_parser_parse((TSParser*)payload, NULL,
      reader.asTsInput());

      TSNode root = ts_tree_root_node(tree); 

      cout << file.path << endl;
      cout << ts_node_string(root) << endl; 
    }

  }
  return DirWalker::ACTION::CONTINUE;
}

void singleThreaded(std::string path, TSParser *parser) {
  DirWalker walker(path);
  if (walker.isValid()) {
    walker.recursive = true;
    walker.walk(printActionSync, parser);
  } else {
    std::cerr << "Failed to open directory.\n";
  }
};

int main(int argc, char *argv[]) {
  if(argc > 1){
     TSParser *parser = ts_parser_new();
     ts_parser_set_language(parser, tree_sitter_java());
     singleThreaded(argv[1], parser);
  }
  return 0;
}
