#ifndef TS_QUERIES_H
#define TS_QUERIES_H

#include <string>

namespace ts::java::queries {

using std::string;

/* All method names */
static const string METHOD_IDENTIFIER = R"(
(
  method_declaration
    name: (identifier) @method.name
)
)";

/* All constructor names */
static const string CONSTRUCTORS = R"(
(
  constructor_declaration
    name: (identifier) @constructor.name
)
)";

/* Methods + constructors (functions) */
static const string FUNCTIONS = R"(
(
  method_declaration
    name: (identifier) @function.name
)

(
  constructor_declaration
    name: (identifier) @function.name
)
)";

/* ============================================================
 * TYPE DECLARATIONS
 * ============================================================
 */

/* Class names */
static const string CLASSES = R"(
(
  class_declaration
    name: (identifier) @class.name
)
)";

/* Interface names */
static const string INTERFACES = R"(
(
  interface_declaration
    name: (identifier) @interface.name
)
)";

/* Enum names */
static const string ENUMS = R"(
(
  enum_declaration
    name: (identifier) @enum.name
)
)";

/* ============================================================
 * FIELDS & VARIABLES
 * ============================================================
 */

/* Field declarations */
static const string FIELDS = R"(
(
  field_declaration
    declarator: (variable_declarator
      name: (identifier) @field.name
    )
)
)";

/* Local variables */
static const string LOCAL_VARIABLES = R"(
(
  local_variable_declaration
    declarator: (variable_declarator
      name: (identifier) @variable.name
    )
)
)";

/* Method parameters */
static const string PARAMETERS = R"(
(
  formal_parameter
    name: (identifier) @parameter.name
)
)";

/* ============================================================
 * IMPORTS & PACKAGES
 * ============================================================
 */

/* Package declaration */
static const string PACKAGE = R"(
(
  package_declaration
    (scoped_identifier) @package.name
)
)";

/* Import declarations */
static const string IMPORTS = R"(
(
  import_declaration
    (scoped_identifier) @import.name
)
)";

/* ============================================================
 * METHOD INVOCATIONS
 * ============================================================
 */

/* All method calls */
static const string METHOD_CALLS = R"(
(
  method_invocation
    name: (identifier) @call.name
)
)";

/* Qualified calls: obj.method() */
static const string QUALIFIED_METHOD_CALLS = R"(
(
  method_invocation
    object: (_)
    name: (identifier) @call.name
)
)";

/* ============================================================
 * ANNOTATIONS
 * ============================================================
 */

/* Simple annotations */
static const string ANNOTATIONS = R"(
(
  annotation
    name: (identifier) @annotation.name
)
)";

/* Qualified annotations */
static const string QUALIFIED_ANNOTATIONS = R"(
(
  annotation
    name: (scoped_identifier) @annotation.name
)
)";

/* ============================================================
 * MODIFIER-BASED QUERIES (UTILITY)
 * ============================================================
 */

/* Public methods */
static const string PUBLIC_METHODS = R"(
(
  method_declaration
    modifiers: (modifiers (modifier) @m (#eq? @m "public"))
    name: (identifier) @method.name
)
)";

/* Static methods */
static const string STATIC_METHODS = R"(
(
  method_declaration
    modifiers: (modifiers (modifier) @m (#eq? @m "static"))
    name: (identifier) @method.name
)
)";

static const string METHOD_WITH_PARAMETERS = R"(
(
  method_declaration
    name: (identifier) @method.name
    parameters: (formal_parameters
      (formal_parameter
        name: (identifier) @parameter.name
      )
    )
)
)";

/* ============================================================
 * ADVANCED / STRUCTURAL QUERIES
 * ============================================================
 */

/* Methods scoped inside classes */
static const string CLASS_METHODS = R"(
(
  class_declaration
    name: (identifier) @class.name
    body: (class_body
      (method_declaration
        name: (identifier) @method.name
      )
    )
)
)";

/* Public API surface (public classes + methods) */
static const string PUBLIC_API = R"(
(
  class_declaration
    modifiers: (modifiers (modifier) @m (#eq? @m "public"))
    name: (identifier) @class.name
)

(
  method_declaration
    modifiers: (modifiers (modifier) @m (#eq? @m "public"))
    name: (identifier) @method.name
)
)";

/* Control-flow blocks (useful for analysis) */
static const string CONTROL_FLOW = R"(
(if_statement) @control.if
(for_statement) @control.loop
(enhanced_for_statement) @control.loop
(while_statement) @control.loop
(do_statement) @control.loop
)";

} // namespace ts::java::queries

#endif // TS_QUERIES_H

