#include "git2/signature.h"
#include <cctype>
#include <git2.h>
#include <iostream>
#include <lib.hpp>
#include <string>
#include <tree_sitter/api.h>
#include <assert.h>

using namespace std;

extern "C" {
    const TSLanguage *tree_sitter_java(void);
}

string from = "Query q = getAm().createQuery(\"hql\");";
string to = "Query<E> q = getAm().createQuery(\"hql\", E);";

void insert_type(FileEditor& e, CSTTree& t, TSNode n, const string& typeName){
    auto r = TSEngine::getRange(n);
    TSRange change = { r.start_point, r.start_point, r.start_byte + 5, r.start_byte + 5 }; // after "Query"
    string insert_text = "<" + typeName + ">";
    e.queue({FileEditor::INSERT, change, { "", insert_text }});
    e.queue({FileEditor::PRINT_CHANGE, change, {"", insert_text}});
}

void insert_klass(FileEditor& e, CSTTree& t, TSNode n, const string& typeName){
    auto r = TSEngine::getRange(n);
    TSRange change = { r.end_point, r.end_point, r.end_byte, r.end_byte };
    string insert_text = ", " + typeName + ".class";
    e.queue({FileEditor::INSERT, change, {"", insert_text}});
    e.queue({FileEditor::PRINT_CHANGE, change, {"", insert_text}});
}

// Recursively collect all string literals from a binary_expression
string collect_string_literals(CSTTree& t, TSNode n){
    if(ts_node_type(n) == "string_literal") {
        return t.getText(n); // include quotes
    } else if(ts_node_type(n) == "binary_expression") {
        string left = collect_string_literals(t, ts_node_child(n, 0));
        string right = collect_string_literals(t, ts_node_child(n, 2));
        return left + right;
    }
    return "";
}

// Parse HQL to extract entity type
string extract_entity_from_hql(CSTTree& t, TSNode n){
    string hql;
    if(ts_node_type(n) == "string_literal") {
        hql = t.getText(n);
    } else if(ts_node_type(n) == "binary_expression") {
        hql = collect_string_literals(t, n);
    }

    // convert to lowercase for case-insensitive search
    string lower_hql = hql;
    transform(lower_hql.begin(), lower_hql.end(), lower_hql.begin(), ::tolower);

    size_t pos = lower_hql.find("from ");
    if(pos != string::npos){
        pos += (lower_hql[pos] == 'f') ? 5 : 12;
        size_t end = hql.find_first_of(" \";", pos);
        return hql.substr(pos, end - pos);
    }

    return "TODO_ENTITY_NAME"; // fallback
}

int main(int argc, char** argv){
    string path = argv[1];
    ThreadPool pool;
    DirWalker walker(path);
    walker.recursive = true;
    const TSLanguage *lang = tree_sitter_java();

    string qf = R"( 
    (local_variable_declaration
        type: (type_identifier) @query_type
        (variable_declarator
            (identifier) @var
            (method_invocation
                (method_invocation
                    (method_invocation
                        (identifier) @method_name
                        (argument_list
                            [ (string_literal) @simple_q
                              (binary_expression) @concat_q
                            ] @query
                        )
                    )
                )
            )
        )
    )
    )";

    walker.walk([lang, &qf](DirWalker::STATUS s, File f) {
        if(s == DirWalker::QUEUING) return DirWalker::CONTINUE;
        if(f.ext != ".java") return DirWalker::CONTINUE;

        FileWriter w(f);
        FileEditor edt;
        TSEngine eng(lang);
        thread_local TSQuery* q = eng.queryNew(qf);
        CSTTree t = eng.parse(w);

        t.find(q, [&w, &t, &edt](TSQueryMatch match) mutable{
            string entity_type;

            // first extract entity type from query
            for(size_t i = 0; i < match.capture_count; i++ ){
                TSNode n = match.captures[i].node;
                switch(match.captures[i].index){
                    case 4: // query
                        entity_type = extract_entity_from_hql(t, n);
                        break;
                }
            }

            // apply insertions
            for(size_t i = 0; i < match.capture_count; i++ ){
                TSNode n = match.captures[i].node;
                switch(match.captures[i].index){
                    case 1: // Query
                        insert_type(edt, t, n, entity_type);
                        break;
                    case 5: // klass
                        insert_klass(edt, t, n, entity_type);
                        break;
                    default:
                        break;
                }
            }

        });

        edt.queue({FileEditor::OP::VALIDATE_CST, {},{f.pathStr}});
        auto errors = edt.apply(t, w);
        edt.reset();
        return DirWalker::CONTINUE;
    });

    pool.waitUntilFinished();
    return 0;
}
