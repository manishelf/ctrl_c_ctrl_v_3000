#include <iostream>
#include <lib.h>
#include <mutex>
#include <string>

void multiThreaded(std::string path) {
  // 1. Initialize the ThreadPool
  // We use hardware_concurrency to match the number of CPU cores
  size_t threads = std::thread::hardware_concurrency();
  ThreadPool pool(threads == 0 ? 4 : threads);

  // Mutex to synchronize console output so text doesn't overlap
  std::mutex consoleMtx;

  // 2. Define the Action
  // This lambda will be called for every file found
  auto printActionAsync = [&](DirWalker::STATUS status, tinydir_file file) {
    if (status == DirWalker::OPENED && !file.is_dir) {

      // We enqueue a task to the pool to read and print the file
      pool.enqueue([&consoleMtx, file]() {
        // Lock the console so one file prints at a time
        std::lock_guard<std::mutex> lock(consoleMtx);

        std::cout << "\n--- FILE: " << file.name << std::endl;
      });
    }
    return DirWalker::CONTINUE;
  };

  // 3. Start walking the directory
  std::cout << "Scanning " << path << " directory for files...\n";
  DirWalker walker(path);
  if (walker.isValid) {
    // Recursive = false (just current dir)
    walker.recursive = true;
    walker.walk(pool, printActionAsync);
  } else {
    std::cerr << "Failed to open directory.\n";
  }

  // 4. Wait for all background tasks to finish
  std::cout << "Waiting for threads to finish processing...\n";
  pool.waitFinished();

  std::cout << "\nDone.\n";
}

DirWalker::ACTION printActionSync(DirWalker::STATUS status, tinydir_file file) {
  if (status == DirWalker::OPENED && !file.is_dir) {
    std::cout << "\n--- FILE: " << file.name << std::endl;
  };
  return DirWalker::CONTINUE;
}

void singleThreaded(std::string path) {
  DirWalker walker(path);
  if (walker.isValid) {
    walker.recursive = true;
    walker.walk(printActionSync);
  } else {
    std::cerr << "Failed to open directory.\n";
  }
};

int main(int argc, char *argv[]) {
  if (argc > 1) {
    multiThreaded(argv[1]);
    singleThreaded(argv[1]);
  } else {
    multiThreaded(".");
    singleThreaded(".");
  }
  return 0;
}
