#include <TSEngine.hpp>
#include <Logger.hpp>
#include <CacheAndPool.hpp>

namespace copypasta {
    //CSTTree

    CSTTree::CSTTree(TSTree* tree, std::string_view source, TSEngine* parent)
        : source(source), parent(parent), tree(tree) {
        DEBUG_FULL("CSTTree ctor");
    };

    CSTTree::CSTTree(const CSTTree& other)
        : source(other.source),
        parent(other.parent) {
        DEBUG_FULL("CSTTree copy ctor");
        tree = std::unique_ptr<TSTree, TSTreeDeleter>(ts_tree_copy(other.tree.get()));
    }

    CSTTree::~CSTTree() {
        DEBUG_FULL("CSTTree destroyed");
    };

    std::string CSTTree::asSexpr() {
        DEBUG_FULL("CSTTree asSexpr");
        TSNode node = ts_tree_root_node(tree.get());
        char* raw = ts_node_string(node);
        auto res = std::string(raw);
        free(raw);
        return res;
    };

    void CSTTree::getQueryForNode(TSNode node, std::string& query, size_t level) {
        DEBUG_FULL("CSTTree getQueryForNode");
        query.append(std::string(level, '\t'));
        query.append("(");
        query.append(ts_node_type(node));

        uint32_t count = ts_node_child_count(node);

        for (size_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_named(child))
                continue;
            query.append("\n");
            getQueryForNode(child, query, level + 1);
            query.append(std::string(level, '\t'));
        }

        query.append(")");
        query.append("@");
        query.append(ts_node_type(node));
        query.append("_" + std::to_string(level));
        query.append("\n");
    };

    std::string CSTTree::getText(TSNode n) {
        DEBUG_FULL("CSTTree getText");
        auto sb = ts_node_start_byte(n);
        auto eb = ts_node_end_byte(n);
        return std::string(source.substr(sb, eb - sb));
    };

    std::string CSTTree::asQuery() {
        DEBUG_FULL("CSTTree asQuery");
        std::string query;
        TSNode node = ts_tree_root_node(tree.get());
        getQueryForNode(node, query);
        return query;
    };

    bool CSTTree::validate(const TSInputEdit ed, size_t insertL, size_t delL) {
        size_t size = source.size();

        if (ed.start_byte > size)
            return false;
        if (ed.old_end_byte > size)
            return false;
        if (ed.new_end_byte < ed.start_byte)
            return false;
        if (ed.old_end_byte < ed.start_byte)
            return false;
        if (insertL != 0 || delL != 0) {
            if (ed.old_end_byte != ed.start_byte + delL)
                return false;
            if (ed.new_end_byte != ed.start_byte + insertL)
                return false;
        }

        if (!(ed.start_byte <= ed.old_end_byte && ed.start_byte <= ed.new_end_byte))
            return false;

        return true;
    };

    void CSTTree::edit(const TSInputEdit ed, const std::string_view source) {
        DEBUG("CSTTree edit");
        this->source = source;
        ts_tree_edit(tree.get(), &ed);
        auto newTree = parent->parse(*this, source);
        tree = std::move(newTree.tree);
        newTree.tree = nullptr;
    }

    void CSTTree::sync() {
        DEBUG("CSTTree sync");
        auto newTree = parent->parse(source);
        tree = std::move(newTree.tree);
        newTree.tree = nullptr;
    }

    std::vector<TSRange> CSTTree::getErrors() {
        std::string q = R"(
      [
         (ERROR)
         (MISSING)
      ] @syntax.error
  )";

        DEBUG("CSTTree getErrors start");
        TSQuery* sq = TSQueryCache::global().get(parent, q);
        std::vector<TSRange> errors;
        find(sq, [&errors](TSQueryMatch m) mutable {
            for (size_t i = 0; i < m.capture_count; i++) {
                TSNode n = m.captures[i].node;
                TSRange r = { ts_node_start_point(n), ts_node_end_point(n),
                             ts_node_start_byte(n), ts_node_end_byte(n) };
                errors.push_back(r);
            }
            });

        DEBUG("CSTTree getErrors ends");
        return errors;
    }


    //TSEngine

    TSEngine::TSEngine(const TSLanguage* lang) {
        // TODO: should lang be a shared pointer
        this->lang = lang;
        // TODO: should parser be a shared pointer or should it come from pool
        //
        DEBUG_FULL("TSEngine ctor");
        TSParser* parser = ts_parser_new();
        ts_parser_set_language(parser, lang);
        this->parser = parser;
    };

    TSEngine::~TSEngine() {
        DEBUG_FULL("TSEngine destroyed");
        if (parser) {
            ts_parser_delete(parser);
        }
        parser = nullptr;
    };


    CSTTree TSEngine::parse(FileReader& reader) {
        DEBUG_FULL("TSEngine parse begin");
        TSTree* tree = ts_parser_parse(parser, NULL, reader.asTsInput());
        DEBUG_FULL("TSEngine parse end");
        return CSTTree(tree, reader.get(reader.bufStart, reader.bufSize), this);
    }

    CSTTree TSEngine::parse(FileWriter& writer) {
        auto source = writer.snapshot().cont;
        // TODO: use TSInput here
        DEBUG_FULL("TSEngine parse begin");
        TSTree* tree =
            ts_parser_parse_string(parser, NULL, source.data(), source.length());
        DEBUG_FULL("TSEngine parse end");
        return CSTTree(tree, source, this);
    }

    CSTTree TSEngine::parse(std::string_view source) {
        // TODO: use TSInput here
        DEBUG_FULL("TSEngine parse begin");
        TSTree* tree =
            ts_parser_parse_string(parser, NULL, source.data(), source.length());
        DEBUG_FULL("TSEngine parse end");
        return CSTTree(tree, source, this);
    };

    CSTTree TSEngine::parse(const CSTTree& old, std::string_view source) {
        // TODO: use TSInput here
        DEBUG("TSEngine parse begin");
        TSTree* tree =
            ts_parser_parse_string(parser, old.tree.get(), source.data(), source.length());
        DEBUG("TSEngine parse end");
        return CSTTree(tree, source, this);
    };

    TSQuery* TSEngine::queryNew(std::string& queryExpr) const {
        DEBUG("TSEngine queryNew " << queryExpr);
        uint32_t errorOffset = 0;
        TSQueryError error;

        TSQuery* query = ts_query_new(lang, queryExpr.c_str(), queryExpr.length(),
            &errorOffset, &error);

        if (error != TSQueryErrorNone) {
            const char* errType = "Unknown";
            switch (error) {
            case TSQueryErrorSyntax: errType = "Syntax"; break;
            case TSQueryErrorNodeType: errType = "NodeType"; break;
            case TSQueryErrorField: errType = "Field"; break;
            case TSQueryErrorCapture: errType = "Capture"; break;
            case TSQueryErrorStructure: errType = "Structure"; break;
            case TSQueryErrorLanguage: errType = "Language"; break;
            default: break;
            }
            std::string msg = "TSEngine::queryNew error: type=";
            msg += errType;
            msg += ", offset=" + std::to_string(errorOffset) + ", expr='" + queryExpr + "'";
            LERROR(msg);
            throw std::runtime_error(msg);
        }

        return query;
    };

    std::map<std::string, std::vector<std::string>> TSEngine::getAvailableNodeTypes() {
        std::map<std::string, std::vector<std::string>> result;

        uint32_t symbol_count = ts_language_symbol_count(lang);
        DEBUG_FULL("getAvailableNodeTypes");
        for (uint32_t i = 0; i < symbol_count; ++i) {
            const char* name = ts_language_symbol_name(lang, i);
            TSSymbolType type = ts_language_symbol_type(lang, i);

            if (!name) continue;

            std::string typeStr;
            switch (type) {
            case TSSymbolTypeRegular:
                typeStr = "regular";
                break;
            case TSSymbolTypeAnonymous:
                typeStr = "anonymous";
                break;
            case TSSymbolTypeAuxiliary:
                typeStr = "auxiliary";
                break;
            case TSSymbolTypeSupertype:
                typeStr = "supertype";
                break;
            default:
                typeStr = "unknown";
                break;
            }

            DEBUG_FULL(typeStr << " - " << name);
            result[typeStr].push_back(name);
        }

        return result;
    }

    TSRange TSEngine::getRange(TSNode n) {
        TSRange r = {
           ts_node_start_point(n),
           ts_node_end_point(n),
           ts_node_start_byte(n),
           ts_node_end_byte(n)
        };
        return r;
    };

} // namespace copypasta