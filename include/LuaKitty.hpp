#ifndef LUA_KITTY_HPP
#define LUA_KITTY_HPP

extern "C" {
  #include "lua.h"
}

#include <string>
#include <vector>

#include <lib.hpp>

namespace copypasta {

class LuaExecutor {
  
  lua_State* L;
  void bind();
  void bindFile();
  void bindReader();
  void bindWriter();
  void bindWalk();
  void bindLanguage();
  void bindTree();
  void bindEditor();
  void bindGit();
  void bindLogger();
  void bindHelpers();
  void updateLuaArgs();

  bool watcherRunning = false;
  ThreadPool pool;
  void watchAndExec(const std::string& path, int pollIntervalMs);
public:

  std::vector<std::string> args;
  void addArgs(int argc, char** argv);

  LuaExecutor();
  ~LuaExecutor();
  void exec(std::string path);

  void watchAndExecThreaded(const std::string& path, int pollIntervalMs = 1000);
  void joinWatcher();

};

} // namespace copypasta
  
#endif // LUA_KITTY_HPP
