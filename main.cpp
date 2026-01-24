#include <lib.h> 
#include <iostream>
#include <tinydir.h>
#include <tree_sitter/api.h>

using namespace std;

extern "C" {
const TSLanguage *tree_sitter_java(void);
}

int main(int argc, char *argv[]) {

  cout << "Hello world" << endl;

  /**
   * 3. conditionals
   * 2. threads
   */
  ReplaceEngine engine(tree_sitter_java());
  File file = engine.readFile("sample.java");

  file.print();
  ReplaceASTAwareOpr rep("apache","xapachex", "import_declaration");
  rep.apply(file);

  ReplaceASTAwareOpr rep2("apache","xapachex", "local_variable_declaration");
  rep2.apply(file);
  
  //SaveOpr save;
  //save.apply(file);

  // parseAndPrintFile("sample.java", tree_sitter_java());
  //  printFileContent("sample.txt");
  //  printDirectoryContent("./tree-sitter/lib", true);
  return 0;
}
