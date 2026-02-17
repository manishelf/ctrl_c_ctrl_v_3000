#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <functional>

#include "lib.hpp"

namespace fs = std::filesystem;
using Clock = std::chrono::high_resolution_clock;

static const size_t SMALL_FILE_MB = 10;
static const size_t LARGE_FILE_GB = 10;
static const size_t BLOCK_SIZE = 4096;
static const std::string TEMP_DIR = "./perf_test_tmp";


// =====================================================
// Utility
// =====================================================

template<typename F>
long long measure(const std::string& name, F&& func)
{
    auto start = Clock::now();
    func();
    auto end = Clock::now();

    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << name << " -> " << ms << " ms\n";
    return ms;
}

void generateFile(const std::string& path, size_t bytes)
{
    std::ofstream out(path, std::ios::binary);
    std::string chunk(1024 * 1024, 'A');

    size_t written = 0;
    while (written < bytes) {
        size_t toWrite = std::min(chunk.size(), bytes - written);
        out.write(chunk.data(), toWrite);
        written += toWrite;
    }
}

void generateDirectoryTree(const std::string& root,
                           size_t depth,
                           size_t dirsPerLevel,
                           size_t filesPerDir,
                           size_t fileSizeBytes)
{
    std::function<void(std::string, size_t)> create =
        [&](std::string path, size_t level)
    {
        if (level > depth) return;

        fs::create_directory(path);

        for (size_t f = 0; f < filesPerDir; ++f)
            generateFile(path + "/file_" + std::to_string(f) + ".txt",
                         fileSizeBytes);

        for (size_t d = 0; d < dirsPerLevel; ++d)
            create(path + "/dir_" + std::to_string(d), level + 1);
    };

    create(root, 1);
}

// =====================================================
// Small File Benchmarks
// =====================================================

void benchmarkSmallFile()
{
    std::cout << "\n==== Small File Benchmark ====\n";

    std::string file = TEMP_DIR + "/small.dat";
    generateFile(file, SMALL_FILE_MB * 1024ull * 1024ull);

    FileReader reader(file, BLOCK_SIZE);

    measure("FileReader sync()", [&]() { reader.sync(); });

    measure("Regex find()", [&]() {
        auto matches = reader.find("AAA", true);
        std::cout << "Matches: " << matches.size() << "\n";
    });

    auto snap = reader.snapshot();
    FileWriter writer(snap);

    measure("replaceAll()", [&]() {
        writer.replaceAll("AAA", "BBB").commit();
    });

    measure("Row operations", [&]() {
        writer.insertRow(1, "Inserted line");
        writer.deleteRow(0);
    });

    measure("Random block reads (10k)", [&]() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(
            0, reader.getFile().size - BLOCK_SIZE);

        for (int i = 0; i < 10000; ++i)
            reader.readBlockAt(dist(rng));
    });
}

// =====================================================
// ThreadPool Benchmark
// =====================================================

void benchmarkThreadPool()
{
    std::cout << "\n==== ThreadPool Benchmark ====\n";

    ThreadPool pool(std::thread::hardware_concurrency());
    const size_t TASKS = 200000;

    measure("ThreadPool 200k tasks", [&]() {
        for (size_t i = 0; i < TASKS; ++i) {
            pool.enqueue([] {
                volatile int x = 0;
                x++;
            });
        }
        pool.waitUntilFinished();
    });
}

// =====================================================
// DirWalker Benchmark
// =====================================================

void benchmarkDirWalker()
{
    std::cout << "\n==== DirWalker Benchmark ====\n";

    std::string dir = TEMP_DIR + "/dirwalk";
    fs::create_directory(dir);

    for (int i = 0; i < 5000; ++i)
        std::ofstream(dir + "/file_" + std::to_string(i) + ".txt");

    DirWalker walker(dir);

    measure("DirWalker walk()", [&]() {
        walker.walk([](DirWalker::STATUS, File, void*) {
            return DirWalker::CONTINUE;
        });
    });
}

// =====================================================
// 10GB Single File Stress
// =====================================================

void stressTest10GB()
{
    std::cout << "\n==== 10GB Single File Stress Test ====\n";

    std::string path = TEMP_DIR + "/large_10gb.dat";
    size_t bytes = LARGE_FILE_GB * 1024ull * 1024ull * 1024ull;

    measure("Generate 10GB file", [&]() {
        generateFile(path, bytes);
    });

    FileReader reader(path, 1024 * 1024);

    measure("Streaming read 10GB", [&]() {
        size_t total = 0;
        for (auto it = reader.begin(); it != reader.end(); ++it)
            total += (*it).size;

        std::cout << "Streamed MB: "
                  << total / (1024 * 1024) << "\n";
    });
}

// =====================================================
// Composed Pipeline (Single Thread)
// =====================================================

void benchmarkPipelineSingle()
{
    std::cout << "\n==== Pipeline Single Thread ====\n";

    std::string root = TEMP_DIR + "/pipeline_single";

    generateDirectoryTree(root, 3, 3, 20, 2 * 1024 * 1024);

    size_t totalFiles = 0;
    size_t totalBytes = 0;

    auto start = Clock::now();

    DirWalker walker(root);

    walker.walk([&](DirWalker::STATUS status, File file, void*) {

        if (status != DirWalker::OPENED || !file.isReg)
            return DirWalker::CONTINUE;

        totalFiles++;
        totalBytes += file.size;

        FileReader reader(file.pathStr);
        reader.sync();

        auto snap = reader.snapshot();
        FileWriter writer(snap);

        writer.replaceAll("AAA", "BBB");
        writer.commit();

        return DirWalker::CONTINUE;
    });

    auto end = Clock::now();
    double seconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        / 1000.0;

    std::cout << "Files: " << totalFiles
              << " | Throughput: "
              << (totalBytes / (1024.0 * 1024.0)) / seconds
              << " MB/s\n";
}

// =====================================================
// Composed Pipeline (Multi Thread)
// =====================================================

void benchmarkPipelineMulti()
{
    std::cout << "\n==== Pipeline Multi Thread ====\n";

    std::string root = TEMP_DIR + "/pipeline_multi";

    generateDirectoryTree(root, 3, 3, 20, 2 * 1024 * 1024);

    ThreadPool pool(std::thread::hardware_concurrency());

    std::atomic<size_t> totalFiles{0};
    std::atomic<size_t> totalBytes{0};

    auto start = Clock::now();

    DirWalker walker(root);

    walker.walk(pool, [&](DirWalker::STATUS status, File file, void*) {

        if (status != DirWalker::OPENED || !file.isReg)
            return DirWalker::CONTINUE;

        pool.enqueue([&, file] {

            totalFiles++;
            totalBytes += file.size;

            FileReader reader(file.pathStr);
            reader.sync();

            auto snap = reader.snapshot();
            FileWriter writer(snap);

            writer.replaceAll("AAA", "BBB");
            writer.commit();
        });

        return DirWalker::CONTINUE;
    });

    pool.waitUntilFinished();

    auto end = Clock::now();
    double seconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        / 1000.0;

    std::cout << "Files: " << totalFiles
              << " | Throughput: "
              << (totalBytes / (1024.0 * 1024.0)) / seconds
              << " MB/s\n";
}

// =====================================================
// 10GB Distributed Directory Stress
// =====================================================

void stressDistributedDir()
{
    std::cout << "\n==== 10GB Distributed Directory Stress ====\n";

    std::string root = TEMP_DIR + "/stress_dir";
    fs::create_directory(root);

    size_t fileSize = 50 * 1024 * 1024;
    size_t fileCount = (10ull * 1024 * 1024 * 1024) / fileSize;

    for (size_t i = 0; i < fileCount; ++i)
        generateFile(root + "/big_" + std::to_string(i) + ".dat",
                     fileSize);

    benchmarkPipelineMulti();
}

// =====================================================
// MAIN
// =====================================================

int main(int argc, char** argv)
{
    fs::create_directory(TEMP_DIR);

    if (argc < 2) {
        std::cout << "Usage: ./perf [all|small|threadpool|dir|10gb|"
                     "pipeline-single|pipeline-multi|pipeline-all|stress-dir]\n";
        return 0;
    }

    std::string mode = argv[1];

    try {

        if (mode == "all") {
            benchmarkSmallFile();
            benchmarkThreadPool();
            benchmarkDirWalker();
            benchmarkPipelineSingle();
            benchmarkPipelineMulti();
        }
        else if (mode == "small") benchmarkSmallFile();
        else if (mode == "threadpool") benchmarkThreadPool();
        else if (mode == "dir") benchmarkDirWalker();
        else if (mode == "10gb") stressTest10GB();
        else if (mode == "pipeline-single") benchmarkPipelineSingle();
        else if (mode == "pipeline-multi") benchmarkPipelineMulti();
        else if (mode == "pipeline-all") {
            benchmarkPipelineSingle();
            benchmarkPipelineMulti();
        }
        else if (mode == "stress-dir") stressDistributedDir();
        else {
            std::cout << "Unknown mode.\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    std::cout << "\nCleaning up...\n";
    //fs::remove_all(TEMP_DIR);

    return 0;
}
