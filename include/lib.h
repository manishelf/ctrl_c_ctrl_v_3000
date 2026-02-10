#ifndef LIB_H
#define LIB_H

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <tinydir.h>
#include <tree_sitter/api.h>
#include <vector>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

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
// tinydir frees all ascociated files
// when the dir closes
class File {
public:
  std::string path;
  std::string name;
  std::string ext;
  bool isDir;
  bool isReg;
  bool isValid;

#ifndef _MSC_VER
#ifdef __MINGW32__
  struct _stat _s;
#else
  struct stat _s;
#endif
#endif

  File(std::string path);
  File(tinydir_file file);
  File();
  ~File();
};

class FileSnapshot{
  File file;
  std::string data;
  bool isDirty;
  size_t lastModified;
};

class FileReader {
  std::ifstream iFileStream;
  File file;
  void readFileMetadata();
  bool _isValid = false;
  size_t pos;

  static const char *tsRead(void *payload, uint32_t byte_index, TSPoint point,
                            uint32_t *bytes_read);
  char *buf = nullptr;
public:
  size_t level = 0;
  std::vector<size_t> rowOffsets;
  size_t fileSize;
  size_t bufStart;
  size_t bufSize;
  size_t defaultBlockSize;
  bool readReverse;
  FileReader(File file, size_t blockSize = 4096);
  FileReader(std::string filePath, size_t blockSize = 4096);
  ~FileReader();

  bool isValid() { return _isValid; };
  File getFile() { return file; };

  typedef struct {
    char *cont;
    size_t size;
  } block;

  block loadFull();
  block load(size_t from, size_t to);
  std::string_view get(size_t from, size_t to);
  void reset();
  block readBlockAt(size_t pos);
  block next();
  block prev();

  TSInput asTsInput();
  typedef struct {
    TSRange match;
    std::vector<TSRange> captures;
  } MatchResult;

  std::vector<MatchResult> find(std::string pattern, bool regex = false,
                                size_t opt = PCRE2_CASELESS);
  std::vector<MatchResult> findWith(pcre2_code *re,
                                    size_t opt = PCRE2_CASELESS);

  TSPoint getPointFromByte(size_t byteOffset);

  class iterator {
  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = block;
    using difference_type = std::ptrdiff_t;
    using pointer = block *;
    using reference = block &;

    iterator(FileReader *reader, size_t pos) : reader(reader), pos(pos) {}

    value_type operator*() { return reader->readBlockAt(pos); }

    iterator &operator++() {
      pos += reader->defaultBlockSize;
      if (pos >= reader->fileSize)
        pos = reader->fileSize;
      return *this;
    }

    iterator &operator--() {
      if (pos == 0)
        return *this;
      if (pos >= reader->defaultBlockSize)
        pos -= reader->defaultBlockSize;
      else
        pos = 0;
      return *this;
    }

    bool operator==(const iterator &other) const {
      return reader == other.reader && pos == other.pos;
    }

    bool operator!=(const iterator &other) const { return !(*this == other); }

  private:
    FileReader *reader;
    size_t pos;
  };

  iterator begin() { return iterator(this, 0); }

  iterator end() { return iterator(this, fileSize); }

  std::reverse_iterator<iterator> rbegin() {
    return std::reverse_iterator<iterator>(end());
  }

  std::reverse_iterator<iterator> rend() {
    return std::reverse_iterator<iterator>(begin());
  }
};

class FileWriter {
  File file;
  bool _isValid;
  std::ofstream oFileStream;
  std::string buf;
public:
  FileWriter(File file);
  FileWriter(std::string path);
  ~FileWriter();

  bool isValid() { return _isValid; };
  File getFile() { return file; };

  bool backup(const std::string& suffix = ".bak"); // create a backup in same folder
  bool commit(); // save buf to underling file
  bool writeAtomic(const std::string& content); // overwrites entire file content
  bool copy(std::string& path); // read to buf
  bool flush(std::string& path); // create if non existing , will over write existing

  FileWriter& append(std::string cont);
  FileWriter& insert(size_t offset, std::string newCont);
  FileWriter& write(size_t offset, char *newCont, size_t newContLen);
  FileWriter& deleteRow(size_t row);
  FileWriter& insertRow(size_t row, const std::string& line);
  FileWriter& deleteCont(size_t from, size_t to);
  FileWriter& replaceAll(std::string pattern, std::string templateOrResult);
  FileWriter& replace(size_t nth_occ, std::string pattern,
               std::string templateOrResult);
  FileWriter& replaceFirst(std::string pattern, std::string templateOrResult);
  FileWriter& replaceLast(std::string pattern, std::string templateOrResult);
};

class DirWalker {
  tinydir_dir _dir;
  bool _isValid;

public:
  std::string path;
  size_t level = 0;
  bool recursive = false;
  bool includeDotDir = false;
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

  bool isValid() { return _isValid; }

  std::vector<File> allChildren();

  ~DirWalker();

  using WalkAction_t = std::function<ACTION(STATUS, File, void *payload)>;

  STATUS walk(WalkAction_t action, void *payload = nullptr);

  void walk(ThreadPool &pool, WalkAction_t action, void *payload = nullptr);

private:
  void walk(ThreadPool &pool, WalkAction_t action,
            std::shared_ptr<std::atomic<bool>> globalAbort, void *payload);
};

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
    enum Type {
      SIMPLE,
      REGEX,
      CST_QUERY,
      SCRIPTED,
      RENAME_FILE,
      RENAME_DIR,
      COMPOSITE
    };

    const std::string ruleName;
    const Type type;

    void *userData;

    std::string searchPattern; // For SIMPLE/REGEX
    std::string captureQuery;  // For CST_QUERY

    using Hook_t = std::function<void(Transformation *)>;
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

File::File(std::string path) {
  tinydir_file file;
  if (tinydir_file_open(&file, path.c_str()) != -1) {
    path = std::string(file.path);
    name = std::string(file.name);
    ext = std::string(file.extension);
    isDir = file.is_dir == 1;
    isReg = file.is_reg == 1;
    _s = file._s;
    isValid = true;
  } else {
    isValid = false;
  }
};

File::File(tinydir_file file) {
  path = std::string(file.path);
  name = std::string(file.name);
  ext = std::string(file.extension);
  isDir = file.is_dir == 1;
  isReg = file.is_reg == 1;
  _s = file._s;
  isValid = true;
};

File::File() {};

File::~File() {};

// FileReader
FileReader::FileReader(File file, size_t blockSize)
    : iFileStream(file.path, std::ios::binary | std::ios::ate) {
  file = file;
  defaultBlockSize = blockSize;
  _isValid = !file.isDir;
  readFileMetadata();
};

FileReader::FileReader(std::string filePath, size_t blockSize)
    : iFileStream(filePath.c_str(), std::ios::binary | std::ios::ate) {
  file = File(filePath);
  if (file.isValid) {
    _isValid = true && !file.isDir;
    defaultBlockSize = blockSize;
    readFileMetadata();
  } else {
    defaultBlockSize = 0;
    _isValid = false;
  }
};

void FileReader::readFileMetadata() {
  if (iFileStream.is_open()) {
    iFileStream.clear();
    iFileStream.seekg(0, std::ios::end);
    std::streampos pos = iFileStream.tellg();
    if (pos < 0) {
      fileSize = 0;
    } else {
      fileSize = static_cast<size_t>(pos);
    }
    bufSize = fileSize;
    bufStart = 0;

    iFileStream.clear();
    iFileStream.seekg(0, std::ios::beg);

    if (fileSize == 0) {
      buf = new char[1];
      buf[0] = '\0';
    }
    rowOffsets.reserve(fileSize / 50);
    rowOffsets.push_back(0);   // row no 0
                               //
    const size_t size = 16384; // 16KB buffer
    char buffer[size];

    size_t totalOffset = 0;
    while (iFileStream.read(buffer, size) || iFileStream.gcount() > 0) {
      size_t bytesRead = iFileStream.gcount(); // "EOF" check
      for (size_t i = 0; i < bytesRead; ++i) {
        if (buffer[i] == '\n') {
          rowOffsets.push_back(totalOffset + i + 1);
        }
      }
      totalOffset += bytesRead;
    }
  } else {
    bufSize = 0;
    bufStart = 0;
    fileSize = 0;
    _isValid = false;
  }
};

FileReader::block FileReader::loadFull() {
  if (!_isValid)
    return {nullptr, 0};

  if (buf) {
    delete[] buf;
    bufSize = 0;
  }

  iFileStream.clear();
  iFileStream.seekg(0, std::ios::beg);
  buf = new char[fileSize];
  bufStart = 0;
  bufSize = fileSize;
  iFileStream.read(buf, fileSize);
  return {buf, fileSize};
};

std::string_view FileReader::get(size_t from, size_t to) {
  if (!_isValid)
    return {};

  if (from > fileSize || to > fileSize)
    return {};

  size_t length = to - from;

  if (buf == nullptr || (from > 0 && from < bufStart) ||
      (to < fileSize && to > bufSize + bufStart))
    if (load(from, to).cont == nullptr)
      return {};

  return std::string_view(&buf[from - bufStart], length);
};

FileReader::block FileReader::load(size_t from, size_t to) {
  if (!_isValid)
    return {nullptr, 0};

  if (from > fileSize || to > fileSize || to == 0)
    return {nullptr, 0};

  if (buf) {
    delete[] buf;
    bufSize = 0;
  }

  size_t length = to - from;
  buf = new char[length];
  iFileStream.clear();
  iFileStream.seekg(from, std::ios::beg);

  if (!iFileStream.read(buf, length)) {
    delete[] buf;
    bufSize = 0;
    return {nullptr, 0};
  }

  bufStart = from;
  bufSize = length;

  return {buf, length};
};

FileReader::block FileReader::readBlockAt(size_t pos) {
  if (!_isValid)
    return {nullptr, 0};
  if (pos >= fileSize)
    return {nullptr, 0};

  size_t size = std::min(defaultBlockSize, fileSize - pos);

  if (!buf || pos < bufStart || pos + size > bufStart + bufSize) {
    load(pos, pos + size);
    bufStart = pos;
  }

  return {buf + (pos - bufStart), size};
}

TSInput FileReader::asTsInput() {
  TSInput input;
  input.payload = this;
  input.read = &FileReader::tsRead;
  input.encoding = TSInputEncodingUTF8;
  return input;
};

const char *FileReader::tsRead(void *payload, uint32_t byte_index,
                               TSPoint position, uint32_t *bytes_read) {
  auto *reader = static_cast<FileReader *>(payload);

  if (byte_index >= reader->fileSize) {
    *bytes_read = 0;
    return nullptr;
  }

  size_t blockSize =
      std::min(reader->defaultBlockSize, reader->fileSize - byte_index);

  // Ensure buffer covers requested range
  if (!reader->buf || byte_index < reader->bufStart ||
      byte_index + blockSize > reader->bufStart + reader->bufSize) {

    reader->load(byte_index, byte_index + blockSize);
    reader->bufStart = byte_index;
  }

  *bytes_read = static_cast<uint32_t>(blockSize);
  return reader->buf + (byte_index - reader->bufStart);
}

std::vector<FileReader::MatchResult> FileReader::find(std::string pattern,
                                                      bool regex, size_t opt) {

  std::vector<MatchResult> matches;

  if (regex) {
    PCRE2_SPTR pcrePattern = (PCRE2_SPTR)pattern.c_str();

    int errornumber;
    PCRE2_SIZE erroroffset;

    pcre2_code *re = pcre2_compile(pcrePattern, PCRE2_ZERO_TERMINATED, opt,
                                   &errornumber, &erroroffset, NULL);
    if (re == NULL) {
      PCRE2_UCHAR buffer[256];
      pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
      throw std::invalid_argument(
          "could not compile provided regex for fn find " + pattern + " - " +
          file.path);
    }

    if (buf == nullptr)
      if (loadFull().cont == nullptr)
        return matches;

    matches = findWith(re, opt);

    pcre2_code_free(re);

    return matches;
  } else {

    if (buf == nullptr)
      if (loadFull().cont == nullptr)
        return matches;

    std::string_view searchSpace(buf, bufSize);

    size_t foundPos = 0;
    size_t offset = 0;
    while ((foundPos = searchSpace.find(pattern, offset)) !=
           std::string_view::npos) {

      size_t matchStart = foundPos;
      size_t matchEnd = matchStart + pattern.size();

      TSRange range;
      range.start_byte = static_cast<uint32_t>(matchStart);
      range.end_byte = static_cast<uint32_t>(matchEnd);
      range.start_point = getPointFromByte(matchStart);
      range.end_point = getPointFromByte(matchEnd);
      MatchResult match;
      match.match = range;
      matches.push_back(match);

      offset = matchEnd;
    }
    return matches;
  }
};

std::vector<FileReader::MatchResult> FileReader::findWith(pcre2_code *re,
                                                          size_t opt) {

  std::vector<MatchResult> matches;

  if (buf == nullptr)
    if (loadFull().cont == nullptr)
      return matches;

  PCRE2_SPTR subject = (PCRE2_SPTR)buf;
  PCRE2_SIZE subject_length = bufSize;
  PCRE2_SIZE *ovector;

  pcre2_match_data *match_data;
  match_data = pcre2_match_data_create_from_pattern(re, NULL);

  int rc = 0;
  PCRE2_SIZE startOffset = 0;
  while ((rc = pcre2_match(re, subject, subject_length, startOffset, 0,
                           match_data, NULL)) > 0) {
    ovector = pcre2_get_ovector_pointer(match_data);

    MatchResult match;
    TSRange range;

    range.start_byte = static_cast<uint32_t>(ovector[0]);
    range.end_byte = static_cast<uint32_t>(ovector[1]);

    range.start_point = getPointFromByte(range.start_byte);
    range.end_point = getPointFromByte(range.end_byte);

    match.match = range;

    for (int i = 1; i < rc; i++) {
      PCRE2_SIZE start = ovector[2 * i];
      PCRE2_SIZE end = ovector[2 * i + 1];

      if (start == PCRE2_UNSET || end == PCRE2_UNSET)
        continue;

      TSRange capture;
      capture.start_byte = static_cast<uint32_t>(start);
      capture.end_byte = static_cast<uint32_t>(end);
      capture.start_point = getPointFromByte(start);
      capture.end_point = getPointFromByte(end);
      match.captures.push_back(capture);
    }

    startOffset = ovector[1];
    if (ovector[0] == ovector[1]) { // incase of zero length match
      startOffset++;
    }

    if (startOffset >= subject_length)
      break;

    matches.push_back(match);
  };

  pcre2_match_data_free(match_data);
  return matches;
};

TSPoint FileReader::getPointFromByte(size_t byteOffset) {
  // Find the first row offset that is GREATER than our byte
  auto it = std::upper_bound(rowOffsets.begin(), rowOffsets.end(), byteOffset);

  // The row number is the index of the element before 'it'
  uint32_t row = std::distance(rowOffsets.begin(), it) - 1;

  // The column is the difference between our offset and the rows start offset
  uint32_t col = byteOffset - rowOffsets[row];

  return {row, col};
}

FileReader::block FileReader::next() {
  if (!buf || pos >= fileSize || fileSize == 0) {
    return {nullptr, 0};
  }

  size_t currentBlockSize = 0;
  if (fileSize - pos < defaultBlockSize) {
    currentBlockSize = fileSize - pos;
  } else {
    currentBlockSize = defaultBlockSize;
  }

  if (pos < bufStart || pos + currentBlockSize > bufStart + bufSize) {
    load(pos, pos + currentBlockSize);
    bufStart = pos;
  }

  char *currPtr = buf + (pos - bufSize);

  if (readReverse) {
    pos = (pos >= currentBlockSize) ? pos - currentBlockSize : 0;
  } else {
    pos += currentBlockSize;
  }

  return {currPtr, currentBlockSize};
};

FileReader::block FileReader::prev() {
  if (!buf || pos <= 0 || fileSize == 0) {
    return {nullptr, 0};
  }

  size_t currentBlockSize = std::min(defaultBlockSize, pos);
  ;

  if (pos < bufStart || pos + currentBlockSize > bufStart + bufSize) {
    load(pos, pos + currentBlockSize);
    bufStart = pos;
  }

  char *currPtr = buf + (pos - bufSize);

  if (readReverse) {
    if (pos < fileSize - 1) {
      pos += currentBlockSize;
    }
  } else if (pos > 0) {
    pos -= currentBlockSize;
  }

  return {currPtr, currentBlockSize};
};

void FileReader::reset() {
  if (buf) {
    delete[] buf;
    buf = nullptr;
  }
  bufSize = 0;
  bufStart = 0;
  if (readReverse) {
    pos = fileSize;
  } else {
    pos = 0;
  }
};

FileReader::~FileReader() {
  if (iFileStream.is_open()) {
    iFileStream.close();
  }
  if (buf)
    delete[] buf;
};

// FileWriter

FileWriter::FileWriter(File f)
    : oFileStream(file.path, std::ios::out | std::ios::trunc) {
  file = f;
  _isValid = file.isValid;
};

FileWriter::FileWriter(std::string path)
    : oFileStream(path, std::ios::out | std::ios::trunc) {
  file = File(path);
  _isValid = file.isValid;
};

FileWriter::~FileWriter() {
  if (oFileStream.is_open()) 
    oFileStream.close();
};

/*FileWriter& FileWriter::write(std::string newCont) {
  if (!oFileStream.is_open()) {
    _isValid = false;
  }else{
    oFileStream.write(newCont.c_str(), newCont.length());
  }
  return *this;
};

FileWriter& FileWriter::append(std::string cont){
  return *this;
}

bool commit();
bool writeAtomic(const std::string& content);
bool backup(const std::string& suffix);

FileWriter& FileWriter::overWrite(size_t offset, std::string newCont);
FileWriter& FileWriter::overWrite(size_t offset, char *newCont, size_t newContLen);
FileWriter& FileWriter::deleteRow(size_t row);
FileWriter& FileWriter::insertRow(size_t row, const std::string& line);
FileWriter& FileWriter::deleteCont(size_t from, size_t to);
FileWriter& FileWriter::replaceAll(std::string pattern, std::string templateOrResult);
FileWriter& FileWriter::replace(size_t nth_occ, std::string pattern, std::string templateOrResult);
FileWriter& FileWriter::replaceFirst(std::string pattern, std::string templateOrResult); 
FileWriter& FileWriter::replaceLast(std::string pattern, std::string templateOrResult);
*/

// DirWalker
DirWalker::DirWalker(tinydir_dir dir) {
  _dir = dir;
  path = std::string(dir.path);
};

DirWalker::DirWalker(std::string dir) {
  path = dir;
  if (tinydir_open_sorted(&_dir, dir.c_str()) != -1) {
    _isValid = true;
    tinydir_close(&_dir);
  } else {
    _isValid = false;
    Utils::process_tinydir_err("Opening directory: " + path);
  }
};

DirWalker::~DirWalker() {};

std::vector<File> DirWalker::allChildren() {
  std::vector<File> myChildren;

  if (!_isValid)
    return myChildren;
  if (tinydir_open_sorted(&_dir, path.c_str()) == -1) {
    Utils::process_tinydir_err("Opening directory: " + path);
    return myChildren;
  }

  for (size_t i = 0; i < _dir.n_files; i++) {
    tinydir_file f;
    int res = tinydir_readfile_n(&_dir, &f, i);
    if (res != -1) {
      File file(f);
      myChildren.push_back(file);
    } else {
      Utils::process_tinydir_err("Reading file at index " + std::to_string(i) +
                                 " file - " + f.path);
    }
  }

  tinydir_close(&_dir);
  return myChildren;
}

DirWalker::STATUS DirWalker::walk(WalkAction_t action, void *payload) {

  if (!_isValid)
    return FAILED;
  std::vector<File> myChildren = allChildren();

  for (File file : myChildren) {

    ACTION actRes = ACTION::CONTINUE;

    if (!(file.name == "." || file.name == "..") || includeDotDir) {
      actRes = action(STATUS::OPENED, file, payload);
    }

    if (actRes == ACTION::SKIP) {
      continue;
    } else if (actRes == ACTION::STOP) {
      return STATUS::STOPPED;
    } else if (actRes == ACTION::ABORT) {
      return STATUS::ABORTED;
    } else if (actRes == ACTION::CONTINUE && file.isDir && recursive &&
               !(file.name == "." || file.name == "..")) {
      DirWalker child(file.path);
      child.recursive = recursive;
      child.level = level + 1;
      STATUS res = child.walk(action, payload);
      if (res == STATUS::ABORTED) {
        return res;
      } else if (res == STATUS::FAILED) {
        actRes = action(STATUS::FAILED, file, payload);
      }
    }
  }
  return STATUS::DONE;
};

void DirWalker::walk(ThreadPool &pool, WalkAction_t action, void *payload) {
  std::shared_ptr<std::atomic<bool>> abortSignal =
      std::make_shared<std::atomic<bool>>(false);
  walk(pool, action, abortSignal, payload);
}

void DirWalker::walk(ThreadPool &pool, WalkAction_t action,
                     std::shared_ptr<std::atomic<bool>> abortSignal,
                     void *payload) {

  std::vector<File> myChildren = allChildren();
  for (File file : myChildren) {

    // If any thread previously returned ABORT, quit now
    if (abortSignal->load())
      return;

    std::string fileName = std::string(file.name);

    if (fileName == "." || fileName == "..")
      continue;

    ACTION actRes = action(STATUS::QUEUING, file, payload);

    if (actRes == ACTION::STOP)
      return;
    if (actRes == ACTION::SKIP)
      continue;
    if (actRes == ACTION::ABORT) {
      abortSignal->store(true);
      return;
    }

    if (file.isDir && recursive) {
      DirWalker child(file.path);
      child.recursive = recursive;
      child.walk(pool, action, abortSignal, payload);
    } else {

      // create a anonlymous class that has action and file in constructor
      pool.enqueue([action, file, abortSignal, payload]() {
        if (abortSignal->load())
          return;

        ACTION result = action(STATUS::OPENED, file, payload);

        if (result == ACTION::ABORT) {
          abortSignal->store(true);
        }
      });
    }
  }
}

// ThreadPool
ThreadPool::ThreadPool(size_t maxCount) {
  maxCount = maxCount;
  stop = false;
  for (size_t i = 0; i < maxCount; ++i) {
    workers.emplace_back([this] {
      while (true) {
        std::function<void()> job;
        {
          std::unique_lock<std::mutex> lock(queueMutex);
          // Wait until there is a task or we are stopping
          condition.wait(lock, [this] { return stop || !task.empty(); });
          if (stop && task.empty())
            return;
          job = std::move(task.front());
          task.pop();
        }
        job();         // Execute the action
        activeTasks--; // finished
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
