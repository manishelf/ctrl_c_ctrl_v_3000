#include <FileEditor.hpp>
#include <Logger.hpp>
#include <assert.h>

namespace copypasta {

    FileEditor::FileEditor() {

#define GENERATE_MAP(ENUM) OP_STR[ENUM] = #ENUM;
        FOREACH_OP(GENERATE_MAP)
#undef GENERATE_MAP
#define GENERATE_MAP(ENUM) ERROR_STR[ENUM] = #ENUM;
            FOREACH_ERROR(GENERATE_MAP)
#undef GENERATE_MAP
    };

    /*
    ReaderWriterEditorEngineTree getReaderWriterEngineTree(const File& file, const TSLanguage* lang){
       // TODO: these should have move ctor, otherwise they are copied on return
       FileReader fr(file);
       FileWriter fw(fr.snapshot());
       FileEditor edt;
       TSEngine eng(lang);
       CSTTree t = eng.parse(fw);
       return {fr, fw, edt, eng, t};
    }
    */
#define ReaderWriterEngineTree(file,lang)            \
                     FileReader fr(file);            \
                     FileWriter fw(fr.snapshot());   \
                     FileEditor edt;                 \
                     TSEngine eng(lang);             \
                     CSTTree t = eng.parse(fw);      

    FileEditor::Edit FileEditor::queue(FileEditor::Edit e) {
        DEBUG_FULL("FileEditor queue");
        if (e.id == 0) {
            e.id = ++edditIdCounter;
        }
        operations.push_back(e);
        return e;
    };

    bool FileEditor::delEdit(int id) {
        bool deleted = false;
        operations.erase(std::remove_if(operations.begin(), operations.end(),
            [id, &deleted](const Edit& e) {
                bool found = e.id == id;
                if (found) {
                    deleted = true;
                }
                return found;
            }
        ));
        return deleted;
    }

    void FileEditor::reset() {
        DEBUG_FULL("FileEditor queue");
        operations.clear();
        errors.clear();
        edditIdCounter = 0;
        currStep = 0;
    };

    TSPoint FileEditor::getNewEndPoint(const Edit& edit) {

        TSPoint p = edit.range.start_point;

        // OP_DELETE → nothing inserted
        if (edit.op == OP::OP_DELETE) {
            return p;
        }

        if (edit.change.empty()) return p;

        for (char c : edit.change) {
            if (c == '\n') {
                p.row += 1;
                p.column = 0;
            }
            else {
                p.column += 1;
            }
        }

        return p;
    };

    std::vector<FileEditor::Error> FileEditor::getConflictErrors() {

        DEBUG("FileEditor getConflictErrors begins");
        // TODO: this is problematic in terms of perfs as we do two seperate sorts 
        auto op = operations;

        /*
         * scan in ascending order of start offsets.
         * Why? Because once the next edit starts after the current edit ends, no further overlap is possible.
         * This allows an efficient early break in the inner loop.
         */
        std::sort(op.begin(), op.end(),
            [](const FileEditor::Edit& a, const FileEditor::Edit& b) {
                if (a.range.start_byte != b.range.start_byte)
                    return a.range.start_byte < b.range.start_byte;

                return a.range.end_byte < b.range.end_byte;
            });

        // O(n2) can this be O(nlogn)?
        for (size_t i = 0; i < op.size(); ++i) {
            const auto& x = op[i];

            // sort of a selection sort
            for (size_t j = i + 1; j < op.size(); ++j) {
                const auto& y = op[j];

                size_t x1 = x.range.start_byte;
                size_t x2 = x.range.end_byte;
                size_t y1 = y.range.start_byte;
                size_t y2 = y.range.end_byte;
                // conflicts could be - 
                //  x1 y1 y2 x2
                //  y1 x1 x2 y2
                //  x1 y1 x2 y2
                //  y1 x1 y2 x2

                // since sorted by start, no need to check all cases
                //This gives O(n²) worst-case, but the break reduces comparisons in practice.
                if (y1 >= x2) break; // no overlap possible further


                if (NOT_CONFLICTING_OP(x.op) || NOT_CONFLICTING_OP(y.op))
                    continue;

                // overlap exists
                size_t overlap_start = std::max(x1, y1);
                size_t overlap_end = std::min(x2, y2);

                TSRange r;
                r.start_byte = (uint32_t)overlap_start;
                r.end_byte = (uint32_t)overlap_end;

                errors.push_back({ CONFLICT, r, x });
                errors.push_back({ CONFLICT, r, y });
            }
        }
        DEBUG("FileEditor getConflictErrors ends");
        return errors;
    }

    std::vector<FileEditor::Error> FileEditor::step(CSTTree& tree, FileWriter& writer) {
        auto edit = operations[currStep++];
        TSInputEdit te = {
              edit.range.start_byte,    // start_byte
              edit.range.end_byte,      // old_end_byte
              edit.range.end_byte,      // new_end_byte
              edit.range.start_point,   // start_point
              edit.range.end_point,     // old_end_point
              getNewEndPoint(edit),     // new_end_point
        };

        switch (edit.op) {
        case OP_INSERT:
        {
            te.old_end_byte = edit.range.start_byte;
            te.new_end_byte = edit.range.start_byte + (uint32_t)edit.change.length();
            writer.insert(edit.range.start_byte, edit.change);
            tree.edit(te, writer.snapshot().cont);
            break;
        }
        case OP_INSERT_ROW_BEFORE:
        {
            size_t rowByteStart = writer.getRowOffsets()[edit.range.start_point.row];
            size_t insertedLen = edit.change.length();
            // +1 for the '\n' that insertRowBefore appends when not already present
            bool hasNewline = !edit.change.empty() && edit.change.back() == '\n';
            size_t totalLen = hasNewline ? insertedLen : insertedLen + 1;

            writer.insertRowBefore(edit.range.start_point.row, edit.change, true);

            te.start_byte = rowByteStart;
            te.old_end_byte = rowByteStart;
            te.new_end_byte = rowByteStart + totalLen;

            te.start_point = { edit.range.start_point.row, 0 };
            te.old_end_point = { edit.range.start_point.row, 0 };
            te.new_end_point = { edit.range.start_point.row + 1, 0 };

            tree.edit(te, writer.snapshot().cont);
            break;
        }
        case OP_INSERT_ROW_AFTER:
        {
            size_t rowByteStart = writer.getRowOffsets()[edit.range.end_point.row + 1];
            size_t insertedLen = edit.change.length();

            bool hasNewline = !edit.change.empty() && edit.change.back() == '\n';
            size_t totalLen = hasNewline ? insertedLen : insertedLen + 1;

            writer.insertRowAfter(edit.range.end_point.row, edit.change, true);

            te.start_byte = rowByteStart;
            te.old_end_byte = rowByteStart;
            te.new_end_byte = rowByteStart + totalLen;

            te.start_point = { edit.range.end_point.row + 1  , 0 };
            te.old_end_point = { edit.range.end_point.row + 1  , 0 };
            te.new_end_point = { edit.range.end_point.row + 1 + 1, 0 };

            tree.edit(te, writer.snapshot().cont);
            break;
        }
        case OP_DELETE:
        {
            te.old_end_byte = edit.range.end_byte;
            writer.deleteCont(edit.range.start_byte, edit.range.end_byte);
            tree.edit(te, writer.snapshot().cont);
            break;
        }
        case OP_WRITE:
        {
            te.old_end_byte = edit.range.end_byte;
            te.new_end_byte = edit.range.start_byte + (uint32_t)edit.change.length();
            writer.write(edit.range.start_byte, edit.range.end_byte, edit.change);
            tree.edit(te, writer.snapshot().cont);
            break;
        }
        case OP_REPLACE:
        {
            writer.replaceAll(edit.change, edit.context);
            tree.sync(); // substitutions may occur anywhere
            break;
        }
        case OP_MARK:
        {
            size_t rowStart = edit.range.start_point.row;
            size_t rowEnd = edit.range.end_point.row;
            std::string text = edit.change;
            std::string additional = edit.context;

            struct MarkInsert {
                size_t row;
                std::string text;
                std::string tag;
            };

            MarkInsert ops[] = {
              {rowEnd + 1, text, "END"}, // After
              {rowStart, additional, "INFO"},
              {rowStart, text, "START"}
            };

            for (auto& op : ops)
            {
                if (op.text.empty()) { // additional info
                    continue;
                }

                size_t byteStart = writer.getRowOffsets()[op.row];
                size_t byteEndOld = byteStart;

                writer.insertRowBefore(op.row, op.text + " " + op.tag, true);

                size_t byteEndNew = writer.getRowOffsets()[op.row + 1];

                TSInputEdit te1 = te;
                te1.start_byte = byteStart;
                te1.old_end_byte = byteEndOld;
                te1.new_end_byte = byteEndNew;

                te1.start_point.row = op.row;
                te1.start_point.column = 0;

                te1.old_end_point.row = op.row;
                te1.old_end_point.column = 0;

                te1.new_end_point.row = op.row + 1;
                te1.new_end_point.column = 0;

                tree.edit(te1, writer.snapshot().cont);
            }

            break;
        }
        case OP_VALIDATE_CST: {
            for (auto& err : tree.getErrors()) {
                // Find the modifying edit whose range overlaps this CST error
                Edit causingEdit = edit; // fallback to the validate edit itself
                for (auto& op : operations) {
                    if (NOT_CONFLICTING_OP(op.op)
                        || (op.op == OP::OP_VALIDATE_CST)) continue;

                    bool overlaps = op.range.start_byte <= err.end_byte &&
                        op.range.end_byte >= err.start_byte;
                    if (overlaps) {
                        causingEdit = op;
                        break;
                    }
                }
                errors.push_back({ CST_ERROR, err, causingEdit });
            }
            break;
        }
        case OP_PRINT_PATH:
        {
            INFO("FileEditor current path - \n" << writer.getFile().pathStr << ":"
                << edit.range.start_point.row + 1 << ":"
                << edit.range.start_point.column + 1);
            break;
        }
        case OP_PRINT_CHANGE_BEFORE:
        case OP_PRINT_CHANGE_AFTER:
        {
            const auto pOld = edit.range.start_point;
            const auto pNew = getNewEndPoint(edit);

            FileReader r(writer.snapshot());

            const auto& path = writer.getFile().pathStr;

            // one based                        
            const auto startRow = pOld.row + 1;
            const auto startCol = pOld.column + 1;
            const auto endRow = pNew.row + 1;
            const auto endCol = pNew.column + 1;

            const auto oldStart = r.getRowOffsets()[edit.range.start_point.row] + edit.range.start_point.column;
            const auto oldEnd = r.getRowOffsets()[edit.range.end_point.row] + edit.range.end_point.column;

            const std::string_view originalText = r.get(oldStart, oldEnd);

            INFO("-----------------------------------------------------------------");
            INFO("\n" << path << ":" << startRow << ":" << startCol);
            INFO("range: " << startRow << ":" << startCol
                << " -> " << endRow << ":" << endCol);
            INFO(this->OP_STR[edit.op] << " : ");
            INFO(edit.context);

            INFO("<<<<<<<<");
            INFO("\n" << originalText);
            INFO("========");
            INFO(edit.change);
            INFO(">>>>>>>>");

            INFO("-----------------------------------------------------------------");

            break;
        }
        case OP_PRINT_ERRORS:
        {
            for (size_t i = 0; i < errors.size(); ++i) {
                const auto& err = errors[i];
                LERROR("\n" << writer.getFile().pathStr << ":"
                    << err.range.start_point.row + 1 << ":"
                    << err.range.start_point.column + 1);

                switch (err.e) {
                case CONFLICT:
                {
                    const auto& errX = errors[i];
                    const auto& errY = errors[i + 1];

                    size_t x1 = errX.edit.range.start_byte;
                    size_t x2 = errX.edit.range.end_byte;
                    size_t y1 = errY.edit.range.start_byte;
                    size_t y2 = errY.edit.range.end_byte;

                    size_t overlap_start = errX.range.start_byte;
                    size_t overlap_end = errX.range.end_byte;

                    LERROR("CONFLICT detected :");

                    LERROR("  Edit X (edit - " << errX.edit.id << ") : [" << x1 << ", " << x2 << "] -> \""
                        << errX.edit.change << "\"");

                    LERROR("  Edit Y (edit - " << errY.edit.id << ") : [" << y1 << ", " << y2 << "] -> \""
                        << errY.edit.change << "\"");

                    LERROR("  Overlap: [" << overlap_start << ", "
                        << overlap_end << "]\n");

                    i++;
                    break;
                }
                case CST_ERROR:
                {
                    LERROR("CST_ERROR (edit - " << err.edit.id << ") :");

                    LERROR("  Range: ["
                        << err.range.start_point.row + 1 << ":" << err.range.start_point.column
                        << " , "
                        << err.range.end_point.row + 1 << ":" << err.range.end_point.column << "]");

                    LERROR("  Edit : ["
                        << err.edit.range.start_point.row + 1 << ":" << err.edit.range.start_point.column
                        << ", "
                        << err.edit.range.end_point.row + 1 << ":" << err.edit.range.end_point.column
                        << "] -> \""
                        << err.edit.change << "\"\n");
                    break;
                }
                case CST_MISSING:
                {
                    LERROR("CST_MISSING:");

                    LERROR("  Range: ["
                        << err.range.start_byte
                        << ", "
                        << err.range.end_byte
                        << "]");
                    LERROR("  Edit : ["
                        << err.edit.range.start_byte
                        << ", "
                        << err.edit.range.end_byte
                        << "] -> \""
                        << err.edit.change
                        << "\"\n");
                    break;
                }
                default:
                    assert(0 && "NOT_IMPLEMENTED");
                }
            }
            break;
        }
        case OP_SAVE_VALID_ONLY:
        {
            if (writer.snapshot().dirty && errors.empty()) {
                writer.save();
            }
            break;
        }
        case OP_SAVE:
        {
            if (writer.snapshot().dirty) {
                writer.save();
            }
            break;
        }
        case OP_BACKUP:
        {
            if (!edit.change.empty()) {
                writer.backup(edit.change);
            }
            else {
                writer.backup();
            }
            break;
        }
        case OP_WRITE_TO: {
            writer.writeTo(edit.change);
            break;
        }
        default: {
            assert(0 && "NOT IMPLEMENTED");
            break;
        }
        };
        return errors;
    }

    void FileEditor::sortOperations() {
        std::sort(operations.begin(), operations.end(),
            [](const FileEditor::Edit& a, const FileEditor::Edit& b) {
                // desc
                if (a.op != b.op)
                    return a.op > b.op;
                // desc
                if (a.range.start_byte != b.range.start_byte)
                    return a.range.start_byte > b.range.start_byte;
                // desc
                return a.range.end_byte > b.range.end_byte;
            });

    }

    std::vector<FileEditor::Error> FileEditor::applySaveAndMarkErrors(CSTTree& tree, FileWriter& writer) {
        // TODO: fix this. this is behaving VERY wrong or maybe something else is wrong
        queue({ OP_SAVE });
        queue({ OP_VALIDATE_CST });
        auto errs = apply(tree, writer);
        if (!errs.empty()) {
            INFO("FileEditor applySaveAndMarkErrors marking errors - " << errs.size() << " in \n" << writer.getFile().pathStr);
            auto currErrors = errors;
            auto currOp = operations;
            auto currStepCount = currStep;
            auto currEditIdCounter = edditIdCounter;
            reset();
            for (const auto& err : errs) {
                std::string errorTag;
                switch (err.e) {
                case CONFLICT:    errorTag = "//ERROR: CONFLICT"; break;
                case CST_ERROR:   errorTag = "//ERROR: CST_ERROR"; break;
                case CST_MISSING: errorTag = "//ERROR: CST_MISSING"; break;
                default:          errorTag = "//ERROR: UNKNOWN"; break;
                }

                std::string expected = err.edit.change;

                std::string todoMsg = "//TODO: expected - " + expected;
                queue({
                  OP_MARK,
                  err.range,
                  errorTag,
                  todoMsg
                    });
            }
            queue({ OP_SAVE });
            errs = apply(tree, writer);
            reset();
            errors = currErrors;
            operations = currOp;
            currStep = currStepCount;
            edditIdCounter = currEditIdCounter;
        }
        return errs;
    }
    std::vector<FileEditor::Error> FileEditor::apply(CSTTree& tree, FileWriter& writer) {

        DEBUG("FileEditor apply begins");
        // TODO: maybe handle the conflicts based on some priority
        getConflictErrors();

        sortOperations();

        for (size_t i = 0; i < operations.size(); i++) {
            DEBUG("FileEditor apply op - " << operations[i].id);
            step(tree, writer);
        }

        DEBUG("FileEditor apply ends");
        return errors;
    };

} // namespace copypasta

