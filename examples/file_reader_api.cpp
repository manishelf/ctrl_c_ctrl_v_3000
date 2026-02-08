#include <iostream>
#include <map>
#include <algorithm>
#include <lib.h>
#include <iostream>

int main() {
    FileReader reader("./sample.txt");

    std::ifstream file("./sample.txt");

    std::cout << file.is_open() << std::endl;

    if (!reader.isValid()) {
        std::cerr << "Failed to open sample.txt\n";
        return 1;
    }

    std::cout << "File size: " << reader.fileSize << " bytes\n\n";

    // --------- 1. loadFull() ----------
    auto full = reader.loadFull();
    std::cout << "---- loadFull() ----\n";
    std::cout.write(full.cont, full.size);
    std::cout << "\n--------------------\n\n";

    // --------- 2. load(from, to) ----------
    size_t from = 0;
    size_t to   = std::min(reader.fileSize, size_t(64));
    auto slice = reader.load(from, to);
    std::cout << "---- load(0,64) ----\n";
    std::cout.write(slice.cont, slice.size);
    std::cout << "\n--------------------\n\n";

    // --------- 3. readBlockAt() ----------
    std::cout << "==== readBlockAt() ====\n";
    size_t pos = 0;
    while (pos < reader.fileSize) {
        auto block = reader.readBlockAt(pos);
        std::cout << "[BLOCK @" << pos << "] size=" << block.size << "\n";
        std::cout.write(block.cont, block.size);
        std::cout << "\n----\n";
        pos += reader.defaultBlockSize;
    }
    std::cout << "\n";

    // --------- 4. STL forward iteration ----------
    std::cout << "==== Forward iteration ====\n";
    for (const auto& block : reader) {
        std::cout << "[block] size=" << block.size << "\n";
        std::cout.write(block.cont, block.size);
        std::cout << "\n----\n";
    }
    std::cout << "\n";

    // --------- 6. next() / prev() ----------
    reader.reset();
    reader.loadFull();
    std::cout << "==== next() ====\n";
    for (int i = 0; i < 3; ++i) {
        auto b = reader.next();
        std::cout << "[next] size=" << b.size << "\n";
        std::cout.write(b.cont, b.size);
        std::cout << "\n----\n";
    }

    std::cout << "==== prev() ====\n";
    for (int i = 0; i < 3; ++i) {
        auto b = reader.prev();
        std::cout << "[prev] size=" << b.size << "\n";
        std::cout.write(b.cont, b.size);
        std::cout << "\n----\n";
    }
    std::cout << "\n";

    // --------- 7. find() ----------
    std::cout << "==== find(\"lorem\") ====\n";
    auto matches = reader.find("lorem");
    for (const auto& m : matches) {
        size_t start = m.match.start_byte;
        size_t end   = m.match.end_byte;
        auto snippet = reader.load(start, end);
        TSPoint p = reader.getPointFromByte(start);
        std::cout << "Match at byte " << start
                  << " (" << p.row << ":" << p.column << "): ";
        std::cout.write(snippet.cont, snippet.size);
        std::cout << "\n";
    }
    std::cout << "\n";

    // --------- 8. multiple matches per line ----------
    std::cout << "==== Multiple matches per line ====\n";
    std::map<size_t, int> lineCounts;
    for (const auto& m : matches) {
        TSPoint p = reader.getPointFromByte(m.match.start_byte);
        lineCounts[p.row]++;
    }
    for (auto& [line, count] : lineCounts) {
        if (count > 1) {
            std::cout << "Line " << line << " has " << count << " matches\n";
        }
    }

    return 0;
}
