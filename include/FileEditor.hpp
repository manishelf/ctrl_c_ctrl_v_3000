#ifndef FILE_EDITOR_HPP
#define FILE_EDITOR_HPP

#include <FileReaderWriter.hpp>
#include <TSEngine.hpp>
#include <map>
#include <vector>
#include <string>

// order based on increasing precedence 
#define FOREACH_OP(OP)                                                            \
  OP(OP_WRITE_TO)                                                                 \
  OP(OP_SAVE)                                                                     \
  OP(OP_SAVE_VALID_ONLY)                                                          \
  OP(OP_PRINT_PATH)                                                               \
  OP(OP_PRINT_ERRORS)                                                             \
  OP(OP_VALIDATE_CST)                                                             \
  OP(OP_PRINT_CHANGE_AFTER)                                                       \
  OP(OP_MARK)                                                                     \
  OP(OP_WRITE)                                                                    \
  OP(OP_INSERT)                                                                   \
  OP(OP_INSERT_ROW_BEFORE)                                                        \
  OP(OP_INSERT_ROW_AFTER)                                                         \
  OP(OP_REPLACE)                                                                  \
  OP(OP_DELETE)                                                                   \
  OP(OP_PRINT_CHANGE_BEFORE)                                                      \
  OP(OP_BACKUP)                                                                   \

#define NOT_CONFLICTING_OP(op)                 \
  (   op == OP::OP_PRINT_CHANGE_BEFORE         \
   || op == OP::OP_PRINT_CHANGE_AFTER          \
   || op == OP::OP_PRINT_PATH                  \
   || op == OP::OP_PRINT_ERRORS                \
  )   


#define FOREACH_ERROR(ERR)                                                     \
  ERR(CONFLICT)                                                                \
  ERR(CST_ERROR)                                                               \
  ERR(CST_MISSING)

class FileEditor {
public:
  enum OP {
#define GENERATE_ENUM(ENUM) ENUM,
    FOREACH_OP(GENERATE_ENUM)
#undef GENERATE_ENUM
  };

  std::map<OP, std::string> OP_STR;

  enum ERROR {
#define GENERATE_ENUM(ENUM) ENUM,
    FOREACH_ERROR(GENERATE_ENUM)
#undef GENERATE_ENUM
  };

  std::map<ERROR, std::string> ERROR_STR;

  struct Edit {
    OP op;
    TSRange range;
    std::string change;
    std::string context;
    int id = 0;
    std::vector<int> relatedEdits;
  };
  struct Error {
    ERROR e;
    TSRange range;
    Edit edit;
  };
 
  FileEditor();

  Edit queue(Edit e); // unordered
  bool delEdit(int id);
  void reset();
  std::vector<Error> getConflictErrors();
  std::vector<Error> getErrors(){return errors;}
  void sortOperations(); // sorts the operation to have proper edits from bottom to top
  std::vector<Error> step(CSTTree &tree, FileWriter &writer);
  std::vector<Error> apply(CSTTree &tree, FileWriter &writer);
  std::vector<FileEditor::Error> applySaveAndMarkErrors(CSTTree &tree, FileWriter &writer); 
private:
  std::vector<Edit> operations;
  int edditIdCounter = 0;
  int currStep = 0;
  std::vector<Error> errors;

  static TSPoint getNewEndPoint(const Edit& edit);
};

#endif
