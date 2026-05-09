#ifndef TS_ENGINE_HPP
#define TS_ENGINE_HPP

#include <tree_sitter/api.h>
#include <string_view>
#include <string>
#include <vector>
#include <map>

#include <FileReaderWriter.hpp>
#include <Logger.hpp>

namespace copypasta {

class TSEngine;
class CSTTree {
private:
  struct TSTreeDeleter {
    void operator()(TSTree* t) const {
      ts_tree_delete(t); // deletes once after last owner is deleted , previous owner gets null value
    }
  };

  std::unique_ptr<TSTree, TSTreeDeleter> tree;
  std::string_view source;
  TSEngine* parent;

public:
  friend TSEngine;

  CSTTree(TSTree *tree, std::string_view source, TSEngine* parent);
  CSTTree(const CSTTree& other);
  ~CSTTree();

  std::string asSexpr();
  std::string asQuery();
  void getQueryForNode(TSNode node, std::string &query, size_t level = 0);
  std::string getText(TSNode n);

  template <typename cb> 
  void find(TSQuery *query, cb handle) {
      DEBUG("CSTTree find start");
      TSNode root = ts_tree_root_node(tree.get());
      TSQueryCursor *cursor = ts_query_cursor_new();
      ts_query_cursor_exec(cursor, query, root);
      TSQueryMatch match;

      while (ts_query_cursor_next_match(cursor, &match)) {
        DEBUG("CSTTree find handle start");
        handle(match);
        DEBUG("CSTTree find handle end");
      }

      ts_query_cursor_delete(cursor);
      DEBUG("CSTTree find end");
    }

  bool validate(const TSInputEdit edit, size_t insertL = 0, size_t delL = 0);
  void edit(const TSInputEdit edit, const std::string_view source);

  std::vector<TSRange> getErrors();

  // Returns a non-owning pointer to the underlying TSTree.
  // Use ts_tree_copy() if you need an independent lifetime.
  TSTree *getRawTree() const { return tree.get(); }

  TSEngine* getParent() const { return parent; }
  std::string_view getSource() {return source; }
  
  void sync();
};



class TSEngine {
  const TSLanguage *lang;
  TSParser *parser;

public:
  TSEngine(const TSLanguage *lang);
  ~TSEngine();
  CSTTree parse(std::string_view source);
  CSTTree parse(const CSTTree &old, std::string_view modSource);
  CSTTree parse(FileReader &reader);
  CSTTree parse(FileWriter &writer);

  static TSRange getRange(TSNode n);

  TSQuery *queryNew(std::string &queryExpr) const;

  std::map<std::string, std::vector<std::string>> getAvailableNodeTypes();

  const TSLanguage* getRawLang() {return lang;};
  TSParser* getRawParser() {return parser;};

};

} // namespace copypasta

#endif // TS_ENGINE_HPP
