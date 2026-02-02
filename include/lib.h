#ifndef LIB_H
#define LIB_H

#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <tinydir.h>
#include <tree_sitter/api.h>
#include <vector>

// ----------------------------------------------------------
// API
// ----------------------------------------------------------

class Utils {
public:
  static void process_tinydir_err(const std::string &context);
};

class ThreadPool {
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> task;
  std::mutex queueMutex;
  std::condition_variable condition;
  bool stop;
  size_t maxCount;                    // std::thread::hardware_concurrency()
  std::atomic<size_t> activeTasks{0}; // Tracks pending + running tasks

public:
  ThreadPool(size_t maxCount);
  ~ThreadPool();

  // pass in a anonymous class and the action in the constructor will be
  // performed
  template <class F> void enqueue(F &&f);

  bool isBusy() { return activeTasks > 0; }

  // helper to block until all tasks are finished
  // main thread will yield until all threads are done
  void waitFinished() {
    while (activeTasks > 0) {
      std::this_thread::yield(); // Give up CPU slice to workers
    }
  }
};

class FileReader {
  std::ifstream fileStream;

public:
  tinydir_file _file;
  size_t level = 0;
  char *buf = nullptr;
  size_t pos;
  size_t defaultBlockSize;
  size_t fileSize;
  bool hasNext;
  bool readReverse;
  FileReader(tinydir_file file);
  ~FileReader();

  typedef struct {
    char *cont;
    size_t size;
  } block;

  block loadFull();
  void reset();
  block next();
  block prev();
  block load(size_t from, size_t to);
};

class FileWriter {};
class GitUtil {};

class DirWalker {
  tinydir_dir _dir;

public:
  std::string path;
  bool isValid;
  size_t level = 0;
  bool recursive = false;
  enum STATUS {
    QUEUING, // file queued for processing; may be skipped based on action
             // result
    OPENED,  // file is opened for processing
    STOPPED, // Stoped the walk for current dir
    ABORTED, // Stoped the walk altogether
    FAILED,  // Failed to open file or dir
    DONE
  };
  enum ACTION {
    STOP,     // stop walk in current dir
    CONTINUE, // continue walk
    SKIP,     // skip entering child dir
    ABORT     // stop the walk altogether
  };

  DirWalker(tinydir_dir dir);
  DirWalker(std::string dir);

  std::vector<tinydir_file> allChildren();

  ~DirWalker();

  using WalkAction_t = std::function<ACTION(STATUS, tinydir_file)>;

  STATUS walk(WalkAction_t action);

  void walk(ThreadPool &pool, WalkAction_t action);

private:
  void walk(ThreadPool &pool, WalkAction_t action,
            std::shared_ptr<std::atomic<bool>> globalAbort);
};

class TsTree {};


class TsEngine {
private:
  TSLanguage *lang;
  TSParser *parser;
  TSTree *currentTree;

public:
  TsEngine(TSLanguage *lang);
  ~TsEngine();

  class Transformation {
  public:
    enum Type { SIMPLE, REGEX, CST_QUERY, SCRIPTED, RENAME_FILE, RENAME_DIR, COMPOSITE };

    const std::string ruleName;
    const Type type;

    void*  userData;

    std::string searchPattern; // For SIMPLE/REGEX
    std::string captureQuery;  // For CST_QUERY

    using Hook_t = std::function<void(Transformation*)>;
    Hook_t preHook;
    Hook_t postHook;

    // 1. Predicate: Decide if the match is valid before/after transforming
    using Predicate_t = std::function<bool(
        TsEngine *, std::map<std::string, TSNode>, FileReader)>;
    Predicate_t filter;
    Predicate_t validator;


    // 2. Simple Param Returner: Return strings to fill $Key1, $Key2, etc.
    using ParamTransformer_t = std::function<std::map<std::string, std::string>(
        TsEngine *, std::map<std::string, TSNode>, FileReader)>;
    ParamTransformer_t paramTransformer;

    // 3. Raw Text Returner: Return the final string, bypassing templates
    using RawTransformer_t = std::function<std::string(
        TsEngine *, std::map<std::string, TSNode>, FileReader)>;
    RawTransformer_t rawTransformer;

    // 4. Multi-step: A list of sub-transformers to run in sequence
    std::vector<std::shared_ptr<Transformation>> subRules;

    // --- Result Template ---
    std::string resultTemplate;

    Transformation(std::string name, Type t) : ruleName(name), type(t) {}
  };

  class StandardTransformers {};

  typedef struct {
    std::string ruleName;
    TSRange range;
    std::string original_text;
    std::string transformed_text;
    bool isInvalid = false;
  } StagedChange;

  void addRule(Transformation trans);

  void addTransaction(std::vector<Transformation> trans);

  std::vector<StagedChange> transform(FileReader reader);

  // returns valid changes and marks proposed changes that are invalid
  std::vector<StagedChange>
  validateChanges(std::vector<StagedChange> proposedChanges, FileReader reader);

  bool commitChanges(std::vector<StagedChange> changes, FileWriter writer);

  TSNode findDeclaration(TSNode start, const std::string &identifier);

private:
  std::vector<Transformation> transformations;
  std::vector<StagedChange> stagedTransforms;
};

// ----------------------------------------------------------
// IMPL
// ----------------------------------------------------------

// FileReader
FileReader::FileReader(tinydir_file file)
    : fileStream(file.path, std::ios::binary | std::ios::ate) {
  this->_file = file;
  if (fileStream.is_open()) {
    fileSize = fileStream.tellg();
    fileStream.seekg(0, std::ios::beg);

    if (fileSize == 0) {
      buf = new char[1];
      buf[0] = '\0';
    }
  }
};

FileReader::~FileReader() {
  if (fileStream.is_open()) {
    fileStream.close();
  }
  if (buf)
    delete[] buf;
};

FileReader::block FileReader::loadFull() {
  if (buf)
    delete[] buf;

  fileStream.seekg(0, std::ios::beg);
  buf = new char[fileSize];
  fileStream.read(buf, fileSize);

  return {buf, fileSize};
};

FileReader::block FileReader::next() {
  if (!buf || pos >= fileSize || fileSize == 0) {
    return {nullptr, 0};
  }

  char *currPtr = &buf[pos];
  fileStream.seekg(0, std::ios::beg);
  size_t currentBlockSize = 0;
  if (fileSize - pos < defaultBlockSize) {
    currentBlockSize = fileSize - pos;
  } else {
    currentBlockSize = defaultBlockSize;
  }

  if (readReverse && pos > 0) {
    pos -= currentBlockSize;
  } else {
    pos += currentBlockSize;
  }

  return {currPtr, currentBlockSize};
};

FileReader::block FileReader::prev() {
  if (!buf || pos <= 0 || fileSize == 0)
    return {nullptr, 0};

  char *currPtr = &buf[pos];
  size_t currentBlockSize = 0;
  if (fileSize - pos < defaultBlockSize) {
    currentBlockSize = fileSize - pos;
  } else {
    currentBlockSize = defaultBlockSize;
  }

  if (readReverse) {
    if (pos < fileSize - 1) {
      pos += currentBlockSize;
    }
  } else {
    if (pos > 0) {
      pos -= currentBlockSize;
    }
  }
  return {currPtr, currentBlockSize};
};

void FileReader::reset() {
  if (readReverse)
    pos = fileSize - 1;
  else
    pos = 0;
};

FileReader::block FileReader::load(size_t from, size_t to) {

  if (from > fileSize || to > fileSize || to == 0)
    return {nullptr, 0};

  size_t length = to - from;
  buf = new char[length];
  fileStream.seekg(from, std::ios::beg);

  if (!fileStream.read(buf, length)) {
    delete[] buf;
    return {nullptr, 0};
  }

  return {buf, length};
};

// DirWalker
DirWalker::DirWalker(tinydir_dir dir) {
  this->_dir = dir;
  this->path = std::string(dir.path);
};

DirWalker::DirWalker(std::string dir) {
  this->path = dir;
  if (tinydir_open_sorted(&this->_dir, dir.c_str()) != -1) {
    isValid = true;
    tinydir_close(&this->_dir);
  } else {
    isValid = false;
    Utils::process_tinydir_err("Opening directory: " + path);
  }
};

DirWalker::~DirWalker() {};

std::vector<tinydir_file> DirWalker::allChildren() {
  std::vector<tinydir_file> myChildren;

  if (!isValid)
    return myChildren;
  if (tinydir_open_sorted(&_dir, path.c_str()) == -1) {
    Utils::process_tinydir_err("Opening directory: " + path);
    return myChildren;
  }

  for (size_t i = 0; i < _dir.n_files; i++) {
    tinydir_file file;
    int res = tinydir_readfile_n(&_dir, &file, i);
    if (res != -1)
      myChildren.push_back(file);
    else {
      Utils::process_tinydir_err("Reading file at index " + std::to_string(i) +
                                 " file - " + file.path);
    }
  }

  tinydir_close(&_dir);

  return myChildren;
}

DirWalker::STATUS DirWalker::walk(WalkAction_t action) {

  if (!isValid)
    return FAILED;

  std::vector<tinydir_file> myChildren = allChildren();
  for (tinydir_file file : myChildren) {

    ACTION actRes = action(STATUS::OPENED, file);
    std::string fileName = std::string(file.name);

    if (actRes == ACTION::SKIP) {
      // skips opening if it is a child dir
      continue;
    } else if (actRes == ACTION::STOP) {
      return STATUS::STOPPED;
    } else if (actRes == ACTION::ABORT) {
      return STATUS::ABORTED;
    } else if (actRes == ACTION::CONTINUE && file.is_dir && recursive &&
               !(fileName == "." || fileName == "..")) {
      DirWalker child(file.path);
      child.recursive = recursive;
      child.level = this->level + 1;
      STATUS res = child.walk(action);
      if (res == STATUS::ABORTED) {
        return res;
      } else if (res == STATUS::FAILED) {
        actRes = action(STATUS::FAILED, file);
      }
    }
  }
  return STATUS::DONE;
};

void DirWalker::walk(ThreadPool &pool, WalkAction_t action) {
  std::shared_ptr<std::atomic<bool>> abortSignal =
      std::make_shared<std::atomic<bool>>(false);
  this->walk(pool, action, abortSignal);
}

void DirWalker::walk(ThreadPool &pool, WalkAction_t action,
                     std::shared_ptr<std::atomic<bool>> abortSignal) {

  std::vector<tinydir_file> myChildren = allChildren();
  for (tinydir_file file : myChildren) {

    // If any thread previously returned ABORT, quit now
    if (abortSignal->load())
      return;

    std::string fileName = std::string(file.name);

    if (fileName == "." || fileName == "..")
      continue;

    ACTION actRes = action(STATUS::QUEUING, file);

    if (actRes == ACTION::STOP)
      return;
    if (actRes == ACTION::SKIP)
      continue;
    if (actRes == ACTION::ABORT) {
      abortSignal->store(true);
      return;
    }

    if (file.is_dir && recursive) {
      DirWalker child(file.path);
      child.recursive = recursive;
      child.walk(pool, action, abortSignal);
    } else {

      // create a anonlymous class that has action and file in constructor
      pool.enqueue([action, file, abortSignal]() {
        if (abortSignal->load())
          return;

        ACTION result = action(STATUS::OPENED, file);

        if (result == ACTION::ABORT) {
          abortSignal->store(true);
        }
      });
    }
  }
}

// ThreadPool
ThreadPool::ThreadPool(size_t maxCount) {
  this->maxCount = maxCount;
  stop = false;
  for (size_t i = 0; i < maxCount; ++i) {
    workers.emplace_back([this] {
      while (true) {
        std::function<void()> job;
        {
          std::unique_lock<std::mutex> lock(this->queueMutex);
          // Wait until there is a task or we are stopping
          this->condition.wait(
              lock, [this] { return this->stop || !this->task.empty(); });
          if (this->stop && this->task.empty())
            return;
          job = std::move(this->task.front());
          this->task.pop();
        }
        job();               // Execute the action
        this->activeTasks--; // finished
      }
    });
  }
}
ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
  }
  condition.notify_all(); // Wake up all threads to let them finish
  for (std::thread &worker : workers) {
    worker.join(); // Wait for every thread to finish its current job
  }
}

template <class F> void ThreadPool::enqueue(F &&f) {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    activeTasks++;
    task.emplace(std::forward<F>(f));
  }
  condition.notify_one();
}

// Utils
void Utils::process_tinydir_err(const std::string &context) {

  int err = errno; // Global error set by tinydir in C style
  if (err == 0)
    return;

  std::cerr << "[Error] " << context << " | Code: " << err << " | ";

  switch (err) {
  case EACCES:
    std::cerr << "Permission denied. Check read/write privileges.";
    break;
  case ENOENT:
    std::cerr << "No such file or directory. Path might be invalid.";
    break;
  case EMFILE:
  case ENFILE:
    std::cerr << "Too many open files. System handle limit reached.";
    break;
  case ENAMETOOLONG:
    std::cerr << "Path name is too long for the filesystem.";
    break;
  case ENOMEM:
    std::cerr << "Out of memory. Cannot allocate directory buffer.";
    break;
  case ENOTDIR:
    std::cerr << "A component of the path prefix is not a directory.";
    break;
  case ELOOP:
    std::cerr << "Too many symbolic links encountered (Loop).";
    break;
  default:
    std::cerr << std::strerror(err);
    break;
  }
  std::cerr << std::endl;
}

#endif
