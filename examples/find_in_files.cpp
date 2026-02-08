#include <lib.h>
#include<vector>

using namespace std;

DirWalker::ACTION printActionSync(DirWalker::STATUS status, tinydir_file file) {
 if (status == DirWalker::STATUS::OPENED && !file.is_dir) {
    FileReader reader(file);
    std::vector<FileReader::MatchResult> matches = reader.find("createQuery", false );
    for(auto match : matches){
      TSPoint startPoint = match.match.start_point;
      TSPoint endPoint = match.match.start_point;
      std::cout << file.path << "[" << startPoint.row << ":" << startPoint.column <<"]" <<std::endl;
    };
  }
  return DirWalker::ACTION::CONTINUE;
}

void multiThreaded(std::string path) {
  size_t threads = std::thread::hardware_concurrency();
  ThreadPool pool(threads == 0 ? 4 : threads);
  std::mutex consoleMtx;

  DirWalker walker(path);
  if (walker.isValid()) {
    walker.recursive = true;
    walker.walk(pool, printActionSync);
  } else {
    std::cerr << "Failed to open directory.\n";
  }

  std::cout << "Waiting for threads to finish processing...\n";
  pool.waitFinished();

  std::cout << "\nDone.\n";
}


void singleThreaded(std::string path) {
  DirWalker walker(path);
  if (walker.isValid()) {
    walker.recursive = true;
    walker.walk(printActionSync);
  } else {
    std::cerr << "Failed to open directory.\n";
  }
};

int main(int argc, char *argv[]) {
  if(argc > 2){
    cout << argv[2];
    if(strcmp(argv[2], "-s") == 0)
      singleThreaded(argv[1]);
    else
      multiThreaded(argv[1]);
  }
  return 0;
}
