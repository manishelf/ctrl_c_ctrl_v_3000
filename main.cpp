#include <lib.hpp>
#include <LuaKitty.hpp>

using namespace std;
using namespace copypasta;

int main(int argc, char** argv){
  if(argc < 2) {
    cout << "Please provide a lua script to execute" << endl;
    return 1;
  }
  LuaExecutor exec;
  exec.addArgs(argc, argv);
  exec.watchAndExecThreaded(argv[1]);
  exec.joinWatcher();
  return 0;
}

