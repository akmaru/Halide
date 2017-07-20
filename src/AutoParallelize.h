#ifndef HALIDE_AUTO_PARALLELIZE_H
#define HALIDE_AUTO_PARALLELIZE_H

#include "Expr.h"
#include "IR.h"
#include "Polytope.h"

namespace Halide {
namespace Internal {

Stmt auto_parallelize(Stmt s, const Polytope& polytope);

}
}

#endif
