#ifndef HALIDE_FIND_VARIABLE_H
#define HALIDE_FIND_VARIABLE_H

#include <string>
#include "Expr.h"
#include "Scope.h"

namespace Halide {
namespace Internal {

int count_variable(Expr expr, const std::string& vn, bool visit_cond=true);
int count_variable(Expr expr, Scope<Expr>& scope, const std::string& vn, bool visit_cond=true);

// If expr contains variable named vn, return true
// visit_cond is used as switch to track conditional expression in Select IR.
bool find_variable(Expr expr, const std::string& vn);
bool find_variable(Expr expr, Scope<Expr>& scope, const std::string& vn, bool visit_cond=true);

} // Internal
} // Halide

#endif
