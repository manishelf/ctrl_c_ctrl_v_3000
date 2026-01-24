#ifndef LIB_H
#define LIB_H

#include <iostream>
#include <string>
#include <tinydir.h>
#include "lib-x.h"

using namespace std;

enum WALK_RESULT {
  FAIL,
  DONE,
  ABORTED,
  STOPPED
};

enum WALK_ENTRY_STATUS {
  FAILED,
  OPENED,
  CLOSED,
  ONGOING,
};

enum ENTRY_ACTION {
  CONTINUE,
  SKIP, 
  STOP,
  ABORT
};

class TargetEntry {
  public:
  string name;
  string path;
  size_t level;
  tinydir_file file;

  TargetEntry(string path): path(path){
    level = -1;
    name = getNameFromPath(path);
  }
  ~TargetEntry(){}
  private:
  string getNameFromPath(string path){
    string name;
    for(int i = path.length() - 1; i >= 0; --i) {
      if( (path[i] == '/' || path[i] == '\\') && i < path.length() - 1 ) break;
      name+=path[i];
    }

    return string(name.rbegin(), name.rend());
  }
}; 

class TargetFile : public TargetEntry{
  public:
  TargetFile(string path): TargetEntry(path){}
  ~TargetFile() {}
};

typedef ENTRY_ACTION (TargetEntryWalkAction)(WALK_ENTRY_STATUS, TargetEntry&);

class TargetDir: public TargetEntry {
  bool isValidDir;
  public:
  bool isValid() {
     if(isValidDir) return true;

     tinydir_dir dir;
     isValidDir = !(tinydir_open(&dir, this->path.c_str()) == -1);
     tinydir_close(&dir);
     
     return isValidDir;
  }

  TargetDir(string path): TargetEntry(path){
    this->level = 0;
    isValidDir = false;
  }

  WALK_RESULT walk(bool recursive, TargetEntryWalkAction action) {
    tinydir_dir dir;
    if(tinydir_open_sorted(&dir, this->path.c_str()) != -1){

      for(int i = 0; i < dir.n_files; i++){
        tinydir_file file;
        int res = tinydir_readfile_n(&dir, &file, i);
        TargetEntry entry("");
        if(file.is_dir){
          entry = TargetDir(file.path);
        }else{
          entry = TargetFile(file.path);
        }
        entry.level = this->level + 1;

        ENTRY_ACTION actRes = ENTRY_ACTION::CONTINUE;
        if(res == -1){
          actRes = action(WALK_ENTRY_STATUS::OPENED, entry);
        }
        else {
          actRes = action(WALK_ENTRY_STATUS::OPENED, entry);
        }
        if(actRes == ENTRY_ACTION::SKIP){continue;}
        else if(actRes == ENTRY_ACTION::STOP) {break;}
        else if(actRes == ENTRY_ACTION::ABORT) {}
        else if(actRes == ENTRY_ACTION::CONTINUE && file.is_dir && recursive && !(entry.name.compare(".") == 0 || entry.name.compare("..") == 0)){
          TargetDir childDir(file.path);
          childDir.level = this->level + 1;
          WALK_RESULT res = childDir.walk(recursive, action);
          if(res == WALK_RESULT::STOPPED){
          }
          else if(res == WALK_RESULT::ABORTED){
            tinydir_close(&dir);
            return WALK_RESULT::ABORTED;
          }else if(res == WALK_RESULT::FAIL){
            actRes = action(WALK_ENTRY_STATUS::FAILED, entry);       
          }
        }
      }
    
    }else {
      tinydir_close(&dir);
      return WALK_RESULT::FAIL;
    }
    
    tinydir_close(&dir);
    return WALK_RESULT::DONE;
  }

  ~TargetDir(){}
  
};


#endif
