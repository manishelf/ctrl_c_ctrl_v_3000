#ifndef LIB_H
#define LIB_H

#include "lib-x.h"
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <tinydir.h>

using namespace std;

enum WALK_RESULT { FAIL, DONE, ABORTED, STOPPED };

enum WALK_ENTRY_STATE {
  FAILED,
  OPENED,
  CLOSED,
  ONGOING,
};

enum ENTRY_ACTION { CONTINUE, SKIP, STOP, ABORT };

class TargetEntry {
public:
  string name;
  string path;
  size_t level;
  tinydir_file file;
  bool isFile;

  TargetEntry() {};

  TargetEntry(string path) : path(path) {
    level = -1;
    name = getNameFromPath(path);
    isFile = false;
  };

  virtual ~TargetEntry() {};

private:
  string getNameFromPath(string path) {
    string name;
    for (int i = path.length() - 1; i >= 0; --i) {
      if ((path[i] == '/' || path[i] == '\\') && i < path.length() - 1)
        break;
      name += path[i];
    }

    return string(name.rbegin(), name.rend());
  }
};

class TargetFile : public TargetEntry {
  char *contentBuffer = nullptr;
  size_t ptrPosition = 0;
  size_t currentBlockSize = blockSize;
  size_t fileSize = 0;

public:
  bool hasNextBlock = false;
  size_t blockSize = 4096;
  struct Block {
    char *data;
    size_t size;
  };

  bool readReverse = false;

  TargetFile(const std::string &path) : TargetEntry(path) { isFile = true; }

  bool loadFile() {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
      return false;
    }

    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize == 0) {
      contentBuffer = nullptr;
      return true;
    } else {
      hasNextBlock = true;
    }

    if (contentBuffer)
      delete[] contentBuffer;

    contentBuffer = new char[fileSize];
    file.read(contentBuffer, fileSize);
    file.close();

    return true;
  }

  void reset() {
    if (readReverse)
      ptrPosition = fileSize - 1;
    else
      ptrPosition = 0;
    hasNextBlock = fileSize > 0;
    currentBlockSize = blockSize;
  }

  Block next() {
    if (!contentBuffer || ptrPosition >= fileSize || fileSize == 0) {
      hasNextBlock = false;
      return {nullptr, 0};
    }

    char *currPtr = &contentBuffer[ptrPosition];

    if (fileSize - ptrPosition < blockSize) {
      currentBlockSize = fileSize - ptrPosition;
    } else {
      currentBlockSize = blockSize;
    }

    if (readReverse) {
      if (ptrPosition == 0)
        hasNextBlock = false;
      else
        ptrPosition -= currentBlockSize;
    } else {
      ptrPosition += currentBlockSize;
      if (ptrPosition >= fileSize)
        hasNextBlock = false;
    }

    return {currPtr, currentBlockSize};
  }

  Block prev() {
    if (!contentBuffer || ptrPosition <= 0 || fileSize == 0)
      return {nullptr, 0};

    char *currPtr = &contentBuffer[ptrPosition];

    if (fileSize - ptrPosition < blockSize) {
      currentBlockSize = fileSize - ptrPosition;
    } else {
      currentBlockSize = blockSize;
    }

    if (readReverse) {
      if (ptrPosition < fileSize - 1) {
        ptrPosition += currentBlockSize;
      }
    } else {
      if (ptrPosition > 0) {
        ptrPosition -= currentBlockSize;
      }
    }
    return {currPtr, currentBlockSize};
  }

  ~TargetFile() {
    delete[] contentBuffer;
    contentBuffer = nullptr;
  }
};

// typedef ENTRY_ACTION (TargetEntryWalkAction)(WALK_ENTRY_STATUS,
// TargetEntry&);
using TargetEntryWalkAction =
    std::function<ENTRY_ACTION(WALK_ENTRY_STATE, TargetEntry *)>;

class TargetDir : public TargetEntry {
public:
  TargetDir(string path) : TargetEntry(path) {
    this->level = 0;
    isFile = false;
  }

  WALK_RESULT walk(bool recursive, TargetEntryWalkAction action) {
    tinydir_dir dir;
    if (tinydir_open_sorted(&dir, this->path.c_str()) != -1) {

      for (int i = 0; i < dir.n_files; i++) {
        tinydir_file file;
        int res = tinydir_readfile_n(&dir, &file, i);
        TargetEntry* entry = nullptr;
        if (file.is_dir) {
          entry = new TargetDir(file.path);
        } else {
          entry = new TargetFile(file.path);
          }
        entry->file = file;
        entry->level = this->level + 1;

        ENTRY_ACTION actRes = ENTRY_ACTION::CONTINUE;
        if (res == -1) {
          actRes = action(WALK_ENTRY_STATE::OPENED, entry);
        } else {
          actRes = action(WALK_ENTRY_STATE::OPENED, entry);
        }
        if (actRes == ENTRY_ACTION::SKIP) {
          continue;
        } else if (actRes == ENTRY_ACTION::STOP) {
          break;
        } else if (actRes == ENTRY_ACTION::ABORT) {
        } else if (actRes == ENTRY_ACTION::CONTINUE && file.is_dir &&
                   recursive &&
                   !(entry->name.compare(".") == 0 ||
                     entry->name.compare("..") == 0)) {
          TargetDir childDir(file.path);
          childDir.level = this->level + 1;
          WALK_RESULT res = childDir.walk(recursive, action);
          if (res == WALK_RESULT::STOPPED) {
          } else if (res == WALK_RESULT::ABORTED) {
            if(entry) free(entry);
            tinydir_close(&dir);
            return WALK_RESULT::ABORTED;
          } else if (res == WALK_RESULT::FAIL) {
            actRes = action(WALK_ENTRY_STATE::FAILED, entry);
          }

          if(entry) free(entry);
        }
      }

    } else {
      tinydir_close(&dir);
      return WALK_RESULT::FAIL;
    }

    tinydir_close(&dir);
    return WALK_RESULT::DONE;
  }

  ~TargetDir() {}
};

#endif
