#include <lib.h> 
#include <iostream>
#include <tree_sitter/api.h>

using namespace std;

extern "C" {
const TSLanguage *tree_sitter_java(void);
}

ENTRY_ACTION printEntryInDir(WALK_ENTRY_STATUS walkResult, TargetEntry &entry) {
  for(int i = 0; i< entry.level; i++){
    cout << " ";
  }
  cout<<entry.name<<endl;
  return ENTRY_ACTION::CONTINUE;
}

int main(int argc, char *argv[]) {

  cout << "Hello world" << endl;
 
  TargetDir dir("./xyz");
  cout << dir.name <<endl;
  
  cout << dir.walk(true, printEntryInDir) << endl;

  return 0;
}
