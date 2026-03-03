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



string from = "Query<E> q = getAm().createQuery(\"hql\", E);";
string to = "Query q = getAm().createQuery(\"hql\");";



void delete_type(FileEditor& e, CSTTree& t, TSNode n){
  auto r = TSEngine::getRange(n);
  TSRange change = {r.start_point, r.end_point, r.start_byte -1, r.end_byte+1};
  e.queue({FileEditor::DELETE, change});
  e.queue({FileEditor::PRINT_CHANGE, change, {t.getText(n), ""}});
}

void delete_klass(FileEditor& e, CSTTree& t, TSNode n){
  auto r = TSEngine::getRange(n);
  TSRange change = {r.start_point, r.end_point, r.start_byte -1, r.end_byte+1};
  e.queue({FileEditor::DELETE, change});
  e.queue({FileEditor::PRINT_CHANGE, change, {t.getText(n), ""}});
}

int main(int argc, char** argv){
  
  string path = argv[1];
  ThreadPool pool;
  DirWalker walker(path);
  walker.recursive = true;
  const TSLanguage *lang = tree_sitter_java();
  string qf = R"(
(local_variable_declaration
  type: (generic_type
    (type_identifier) @q
    (#eq? @q "Query")
    (type_arguments
      (type_identifier) @type))

   (variable_declarator
     (identifier) @var
     (method_invocation
       (method_invocation
         (method_invocation
           (identifier) @method_name
          (#eq? @method_name "createQuery")

          (argument_list
            [
              (string_literal) @simple_q
              (binary_expression) @concat_q
            ] @query

            (class_literal
              (type_identifier) @klass)
          )
        )
      )
    )
  )
)
                    )";


  walker.walk([lang, &qf](DirWalker::STATUS s, File f) {

    if(s ==DirWalker::QUEUING) return DirWalker::CONTINUE;

    if(f.ext != ".java")
      return DirWalker::CONTINUE;

    FileWriter w(f);
    FileEditor edt; 
    TSEngine eng(lang);

    thread_local TSQuery* q = eng.queryNew(qf);

    CSTTree t = eng.parse(w); 

    t.find(q, [&w, &t, &edt](TSQueryMatch match) mutable{ 
      for(size_t i = 0; i < match.capture_count; i++ ){
        TSNode n = match.captures[i].node;
        switch(match.captures[i].index){
          case 1: // Query 
                  break;
          case 2: // type
                  delete_type(edt, t, n);
                  break;
          case 3: // createQuery
                  break;
          case 4: // query
                  break;
          case 5: // klass
                  delete_klass(edt, t, n);
                  break;
          default : assert("unreachable");
        };
      }
    });

  //  edt.queue({FileEditor::OP::SAVE});
    edt.queue({FileEditor::OP::VALIDATE_CST, {},{f.pathStr}});

    auto errors = edt.apply(t, w);

    for(auto err : errors){
      size_t row , col;
      row = err.range.start_point.row;
      col = err.range.start_point.column;
      //cout << "ERROR:" << edt.ERROR_STR[err.e] << endl;
      //cout << f.pathStr <<":" << row << ":"<< col << endl;
    }

    edt.reset();
  
    return DirWalker::CONTINUE;
  });

  pool.waitUntilFinished();

  return 0;
}

