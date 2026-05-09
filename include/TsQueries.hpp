#ifndef TS_QUERIES_HPP
#define TS_QUERIES_HPP

#include <string>
#include <map>

namespace ts::queries {

using std::string;

// ============================================================
// QUERY ORGANIZATION BY PARADIGM
// ============================================================
// These queries are designed to be language-agnostic where possible.
// Node names may need adaptation based on tree-sitter grammar.
// Use these as templates and adjust node types for your target language.
// ============================================================

namespace oop {
  // Object-Oriented Programming constructs
  
  /* Class declarations - covers: Java, C++, Python, JavaScript, TypeScript, C#, etc. */
  static const string CLASSES = R"(
(class_declaration name: (_) @class.name) @class.declaration
(class_definition name: (_) @class.name) @class.declaration
)";

  /* Interface/Protocol declarations - Java, TypeScript, Go, Swift */
  static const string INTERFACES = R"(
(interface_declaration name: (_) @interface.name) @interface.declaration
(protocol_declaration name: (_) @protocol.name) @protocol.declaration
)";

  /* Abstract classes */
  static const string ABSTRACT_CLASSES = R"(
(class_declaration
  (modifiers)? @modifiers
  name: (_) @class.name
  (#match? @modifiers "abstract")
) @abstract.class
)";

  /* Method declarations - instance methods */
  static const string METHODS = R"(
(method_declaration name: (_) @method.name) @method
(function_definition name: (_) @method.name) @method
)";

  /* Constructors - Java, C++, C#, TypeScript */
  static const string CONSTRUCTORS = R"(
(constructor_declaration name: (_) @constructor.name) @constructor
(constructor_definition name: (_) @constructor.name) @constructor
)";

  /* Inheritance - base classes, superclasses */
  static const string INHERITANCE = R"(
(class_declaration
  name: (_) @class.name
  superclass: (_) @superclass
) @inheritance

(class_definition
  name: (_) @class.name
  bases: (_) @base
) @inheritance
)";

  /* Interface implementations */
  static const string IMPLEMENTS = R"(
(class_declaration
  name: (_) @class.name
  interfaces: (_) @interface
) @implements
)";

  /* Method calls/invocations - obj.method() */
  static const string METHOD_CALLS = R"(
(method_invocation
  object: (_)? @receiver
  name: (_) @method.name
) @method.call

(call_expression
  function: (attribute
    object: (_) @receiver
    attribute: (_) @method.name
  )
) @method.call
)";

  /* Field/Property declarations */
  static const string FIELDS = R"(
(field_declaration
  declarator: (variable_declarator name: (_) @field.name)
) @field

(class_variable name: (_) @field.name) @field
)";

  /* Property access - obj.field */
  static const string PROPERTY_ACCESS = R"(
(field_access
  object: (_) @object
  field: (_) @property
) @access

(attribute
  object: (_) @object
  attribute: (_) @property
) @access
)";

  /* Static members */
  static const string STATIC_MEMBERS = R"(
(method_declaration
  (modifiers)? @modifiers
  name: (_) @method.name
  (#match? @modifiers "static")
) @static.method

(field_declaration
  (modifiers)? @modifiers
  declarator: (variable_declarator name: (_) @field.name)
  (#match? @modifiers "static")
) @static.field
)";

  /* Access modifiers - public, private, protected */
  static const string PUBLIC_MEMBERS = R"(
(method_declaration
  (modifiers)? @modifiers
  name: (_) @member.name
  (#match? @modifiers "public")
) @public.method

(field_declaration
  (modifiers)? @modifiers
  name: (_) @member.name
  (#match? @modifiers "public")
) @public.field
)";

  static const string PRIVATE_MEMBERS = R"(
(method_declaration
  (modifiers)? @modifiers
  name: (_) @member.name
  (#match? @modifiers "private")
) @private.method

(field_declaration
  (modifiers)? @modifiers
  name: (_) @member.name
  (#match? @modifiers "private")
) @private.field
)";

} // namespace oop

namespace functional {
  // Functional Programming constructs

  /* Function declarations - top-level functions */
  static const string FUNCTIONS = R"(
(function_declaration name: (_) @function.name) @function
(function_definition name: (_) @function.name) @function
(function_item name: (_) @function.name) @function
)";

  /* Lambda/Anonymous functions */
  static const string LAMBDAS = R"(
(lambda_expression) @lambda
(arrow_function) @lambda
(closure_expression) @lambda
)";

  /* Higher-order functions - functions taking functions as params */
  static const string HIGHER_ORDER_FUNCTIONS = R"(
(function_declaration
  name: (_) @function.name
  parameters: (formal_parameters
    (formal_parameter
      type: (type_identifier) @param.type
      (#match? @param.type "(Function|Callable|Closure)")
    )
  )
) @higher.order
)";

  /* Function calls */
  static const string FUNCTION_CALLS = R"(
(call_expression
  function: (identifier) @function.name
) @call

(call
  function: (identifier) @function.name
) @call
)";

  /* Pure function candidates - functions with only value params, no mutations */
  static const string PURE_FUNCTION_CANDIDATES = R"(
(function_declaration
  name: (_) @function.name
  parameters: (formal_parameters) @params
  body: (block) @body
  (#not-any-of? @body "assignment" "method_invocation")
) @pure.candidate
)";

  /* Map/Filter/Reduce patterns */
  static const string MAP_FILTER_REDUCE = R"(
(call_expression
  function: (identifier) @fn
  (#any-of? @fn "map" "filter" "reduce" "forEach" "flatMap")
) @functional.iterator
)";

  /* Immutable variable declarations - const, final, val */
  static const string IMMUTABLE_DECLARATIONS = R"(
(local_variable_declaration
  (modifiers)? @modifiers
  (#match? @modifiers "final")
  declarator: (variable_declarator name: (_) @var.name)
) @immutable

(lexical_declaration
  kind: "const"
  declarator: (variable_declarator name: (_) @var.name)
) @immutable
)";

  /* Closures - functions capturing outer scope */
  static const string CLOSURES = R"(
(lambda_expression
  body: (block
    [
      (identifier) @captured
      (field_access object: (identifier) @captured)
    ]
  )
) @closure
)";

  /* Recursive functions - functions calling themselves */
  static const string RECURSIVE_FUNCTIONS = R"(
(function_declaration
  name: (_) @function.name
  body: (block
    (call_expression
      function: (identifier) @call.name
      (#eq? @call.name @function.name)
    )
  )
) @recursive
)";

  /* Pattern matching - match, switch expressions */
  static const string PATTERN_MATCHING = R"(
(match_expression) @pattern.match
(switch_expression) @pattern.match
)";

} // namespace functional

namespace procedural {
  // Procedural/Imperative Programming constructs

  /* Variable declarations */
  static const string VARIABLES = R"(
(variable_declaration
  declarator: (variable_declarator name: (_) @var.name)
) @variable

(local_variable_declaration
  declarator: (variable_declarator name: (_) @var.name)
) @variable

(assignment_expression
  left: (_) @var.name
) @assignment
)";

  /* Mutable variables - var, let */
  static const string MUTABLE_VARIABLES = R"(
(local_variable_declaration
  declarator: (variable_declarator name: (_) @var.name)
) @mutable

(lexical_declaration
  kind: "let"
  declarator: (variable_declarator name: (_) @var.name)
) @mutable
)";

  /* Control flow - if/else */
  static const string CONDITIONALS = R"(
(if_statement
  condition: (_) @condition
  consequence: (_) @then
  alternative: (_)? @else
) @conditional

(if_expression
  condition: (_) @condition
  consequence: (_) @then
  alternative: (_)? @else
) @conditional
)";

  /* Loops - for, while, do-while */
  static const string LOOPS = R"(
(for_statement) @loop.for
(enhanced_for_statement) @loop.foreach
(while_statement) @loop.while
(do_statement) @loop.do
(for_in_statement) @loop.forin
(for_of_statement) @loop.forof
)";

  /* Loop control - break, continue */
  static const string LOOP_CONTROL = R"(
(break_statement) @control.break
(continue_statement) @control.continue
)";

  /* Switch statements */
  static const string SWITCH_STATEMENTS = R"(
(switch_statement
  condition: (_) @switch.condition
  body: (switch_block
    (switch_rule)* @case
  )
) @switch
)";

  /* Goto/Labels (C, C++, assembly) */
  static const string GOTO_LABELS = R"(
(labeled_statement label: (_) @label) @labeled
(goto_statement label: (_) @goto.target) @goto
)";

  /* Assertions */
  static const string ASSERTIONS = R"(
(assert_statement) @assert
(call_expression
  function: (identifier) @fn
  (#any-of? @fn "assert" "assertEqual" "assertTrue")
) @assert.call
)";

  /* State mutations - assignments, increments */
  static const string MUTATIONS = R"(
(assignment_expression
  left: (_) @target
  right: (_) @value
) @mutation

(update_expression
  argument: (_) @target
) @mutation.update
)";

  /* Procedure calls (no return value) */
  static const string PROCEDURE_CALLS = R"(
(expression_statement
  (call_expression
    function: (_) @procedure.name
  )
) @procedure.call
)";

  /* Early returns */
  static const string EARLY_RETURNS = R"(
(return_statement) @return
)";

} // namespace procedural

namespace common {
  // Common constructs across paradigms

  /* All type declarations */
  static const string TYPE_DECLARATIONS = R"(
(class_declaration name: (_) @type.name) @type
(interface_declaration name: (_) @type.name) @type
(enum_declaration name: (_) @type.name) @type
(struct_declaration name: (_) @type.name) @type
(type_alias_declaration name: (_) @type.name) @type
(type_declaration name: (_) @type.name) @type
)";

  /* All function-like constructs */
  static const string ALL_FUNCTIONS = R"(
(function_declaration name: (_) @function.name) @function
(function_definition name: (_) @function.name) @function
(method_declaration name: (_) @function.name) @function
(constructor_declaration name: (_) @function.name) @function
)";

  /* Function/method parameters */
  static const string PARAMETERS = R"(
(formal_parameter
  type: (_)? @param.type
  name: (_) @param.name
) @parameter

(parameter
  type: (_)? @param.type
  name: (_) @param.name
) @parameter
)";

  /* Return statements */
  static const string RETURNS = R"(
(return_statement
  (_)? @return.value
) @return
)";

  /* Comments */
  static const string COMMENTS = R"(
(comment) @comment
(line_comment) @comment.line
(block_comment) @comment.block
)";

  /* Imports/Includes */
  static const string IMPORTS = R"(
(import_declaration
  source: (_)? @import.source
) @import

(include_declaration
  path: (_) @import.path
) @import

(use_declaration) @import

(package_declaration) @package
)";

  /* String literals */
  static const string STRINGS = R"(
(string_literal) @string
(template_string) @string.template
)";

  /* Numeric literals */
  static const string NUMBERS = R"(
(integer_literal) @number.int
(float_literal) @number.float
(decimal_literal) @number
(hex_literal) @number.hex
(binary_literal) @number.binary
)";

  /* Boolean literals */
  static const string BOOLEANS = R"(
(true) @boolean
(false) @boolean
(boolean_literal) @boolean
)";

  /* Null/None/Nil values */
  static const string NULL_VALUES = R"(
(null_literal) @null
(nil) @null
(none) @null
)";

  /* Error handling */
  static const string ERROR_HANDLING = R"(
(try_statement
  body: (_) @try.body
  handler: (_)* @catch
  finalizer: (_)? @finally
) @error.handling

(throw_statement
  (_) @throw.value
) @throw
)";

  /* Annotations/Decorators/Attributes */
  static const string ANNOTATIONS = R"(
(annotation
  name: (_) @annotation.name
) @annotation

(decorator
  name: (_) @decorator.name
) @decorator

(attribute
  name: (_) @attribute.name
) @attribute
)";

  /* Generic/Template types */
  static const string GENERICS = R"(
(type_arguments) @generic.args
(type_parameters) @generic.params
)";

  /* Array/List operations */
  static const string ARRAY_ACCESS = R"(
(array_access
  array: (_) @array
  index: (_) @index
) @access

(subscript_expression
  value: (_) @array
  index: (_) @index
) @access
)";

  /* Binary operations */
  static const string BINARY_OPERATIONS = R"(
(binary_expression
  left: (_) @left
  operator: (_) @operator
  right: (_) @right
) @binary.op
)";

  /* Unary operations */
  static const string UNARY_OPERATIONS = R"(
(unary_expression
  operator: (_) @operator
  operand: (_) @operand
) @unary.op

(update_expression
  operator: (_) @operator
  argument: (_) @operand
) @update.op
)";

  /* Identifiers - variable references */
  static const string IDENTIFIERS = R"(
(identifier) @identifier
)";

  /* TODO/FIXME/NOTE comments */
  static const string TODO_COMMENTS = R"(
(comment) @comment
(#match? @comment "(TODO|FIXME|NOTE|HACK|XXX|BUG)")
)";

  /* Documentation comments */
  static const string DOC_COMMENTS = R"(
(comment) @doc
(#match? @doc "^(///|/\\*\\*|#!|\"\"\")")
)";

} // namespace common

namespace utility {
  // Utility queries for common refactoring/analysis tasks

  /* Dead code candidates - unused private methods */
  static const string UNUSED_PRIVATE_METHODS = R"(
(method_declaration
  (modifiers) @modifiers
  (#match? @modifiers "private")
  name: (_) @method.name
) @unused.candidate
)";

  /* Long methods - methods with many statements */
  static const string COMPLEX_METHODS = R"(
(method_declaration
  name: (_) @method.name
  body: (block) @body
) @complex.method
)";

  /* Magic numbers - numeric literals not in const/final/enum */
  static const string MAGIC_NUMBERS = R"(
(
  [
    (integer_literal)
    (float_literal)
    (decimal_literal)
  ] @magic.number
  (#not-eq? @magic.number "0")
  (#not-eq? @magic.number "1")
)
)";

  /* Empty blocks */
  static const string EMPTY_BLOCKS = R"(
(block) @empty.block
(#eq? @empty.block "{}")
)";

  /* Nested conditionals - if inside if */
  static const string NESTED_CONDITIONALS = R"(
(if_statement
  consequence: (block
    (if_statement) @nested
  )
) @nesting
)";

  /* Test methods - by naming convention */
  static const string TEST_METHODS = R"(
(method_declaration
  name: (identifier) @test.name
  (#match? @test.name "^(test|should|it_)")
) @test.method

(method_declaration
  (annotation
    name: (identifier) @annotation
    (#any-of? @annotation "Test" "test")
  )
  name: (_) @test.name
) @test.method
)";

  /* Deprecated usage */
  static const string DEPRECATED = R"(
(method_declaration
  (annotation
    name: (identifier) @annotation
    (#eq? @annotation "Deprecated")
  )
  name: (_) @deprecated.name
) @deprecated

(call_expression
  function: (identifier) @call
  (#match? @call ".*[Dd]eprecated.*")
) @deprecated.call
)";

  /* Dependency injection candidates - constructor with many params */
  static const string DI_CANDIDATES = R"(
(constructor_declaration
  parameters: (formal_parameters
    (formal_parameter)+ @param
  )
) @di.candidate
)";

  /* God classes - classes with many methods */
  static const string GOD_CLASS_CANDIDATES = R"(
(class_declaration
  name: (_) @class.name
  body: (class_body
    (method_declaration)+ @methods
  )
) @god.class.candidate
)";

} // namespace utility

// Helper: Get query by category and name
inline const string* getQuery(const string& category, const string& name) {
  static std::map<string, std::map<string, const string*>> registry = {
    {"oop", {
      {"classes", &oop::CLASSES},
      {"interfaces", &oop::INTERFACES},
      {"methods", &oop::METHODS},
      {"constructors", &oop::CONSTRUCTORS},
      {"inheritance", &oop::INHERITANCE},
      {"method_calls", &oop::METHOD_CALLS},
      {"fields", &oop::FIELDS},
      {"static_members", &oop::STATIC_MEMBERS},
      {"public_members", &oop::PUBLIC_MEMBERS},
      {"private_members", &oop::PRIVATE_MEMBERS},
    }},
    {"functional", {
      {"functions", &functional::FUNCTIONS},
      {"lambdas", &functional::LAMBDAS},
      {"function_calls", &functional::FUNCTION_CALLS},
      {"higher_order", &functional::HIGHER_ORDER_FUNCTIONS},
      {"pure_candidates", &functional::PURE_FUNCTION_CANDIDATES},
      {"map_filter_reduce", &functional::MAP_FILTER_REDUCE},
      {"immutable", &functional::IMMUTABLE_DECLARATIONS},
      {"recursive", &functional::RECURSIVE_FUNCTIONS},
    }},
    {"procedural", {
      {"variables", &procedural::VARIABLES},
      {"conditionals", &procedural::CONDITIONALS},
      {"loops", &procedural::LOOPS},
      {"mutations", &procedural::MUTATIONS},
      {"early_returns", &procedural::EARLY_RETURNS},
    }},
    {"common", {
      {"types", &common::TYPE_DECLARATIONS},
      {"all_functions", &common::ALL_FUNCTIONS},
      {"parameters", &common::PARAMETERS},
      {"returns", &common::RETURNS},
      {"comments", &common::COMMENTS},
      {"imports", &common::IMPORTS},
      {"error_handling", &common::ERROR_HANDLING},
      {"annotations", &common::ANNOTATIONS},
    }},
    {"utility", {
      {"unused_private", &utility::UNUSED_PRIVATE_METHODS},
      {"complex_methods", &utility::COMPLEX_METHODS},
      {"magic_numbers", &utility::MAGIC_NUMBERS},
      {"test_methods", &utility::TEST_METHODS},
      {"deprecated", &utility::DEPRECATED},
    }},
  };
  
  auto catIt = registry.find(category);
  if (catIt == registry.end()) return nullptr;
  auto nameIt = catIt->second.find(name);
  if (nameIt == catIt->second.end()) return nullptr;
  return nameIt->second;
}

} // namespace ts::queries

#endif // TS_QUERIES_HPP
