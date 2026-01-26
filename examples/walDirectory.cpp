#include <lib.h> 
#include <lib-x.h> 
#include <iostream>
#include <fstream>
#include <tree_sitter/api.h>

using namespace std;

extern "C" {
const TSLanguage *tree_sitter_java(void);
}

ENTRY_ACTION printEntryInDir(WALK_ENTRY_STATE walkState, TargetEntry* entry) {
  for(int i = 0; i< entry->level; i++){
    cout << " ";
  }
  cout<<entry->name<<endl;
  return ENTRY_ACTION::CONTINUE;
}

struct FileContentPrint {
  size_t nPreceding;
  size_t nFollowing;

  ENTRY_ACTION operator()(WALK_ENTRY_STATE walkState, TargetEntry* entry){

    if(entry->isFile){
      cout << entry->name<<endl;
      TargetFile* file = dynamic_cast<TargetFile*>(entry);
      if(file && file->loadFile()){
        while(file->hasNextBlock){
          TargetFile::Block block = file->next();
          string cont(block.data, block.size);
          cout << cont << endl;
        }
      }
    }
    
    return ENTRY_ACTION::CONTINUE;
  }
};


int main(int argc, char *argv[]) {

  cout << "WalkAdirectory" << endl;
 
  TargetDir dir("./xyz");
  cout << dir.name <<endl;  

  cout << dir.walk(true, printEntryInDir) << endl;

  return 0;
}
