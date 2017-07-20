#include <string>

#include "IR.h"
#include "IRVisitor.h"
#include "FindVariable.h"

namespace Halide {
namespace Internal {

class CountVariable : public IRVisitor {
    using IRVisitor::visit;
    Scope<Expr>& scope_;
    const std::string& vn_;
    bool visit_cond_;

    void visit(const Let *op) {
        scope_.push(op->name, op->value);
        op->body.accept(this);
        scope_.pop(op->name);
    }

    void visit(const LetStmt *op) {
        scope_.push(op->name, op->value);
        op->body.accept(this);
        scope_.pop(op->name);
    }

    void visit(const Variable *op) {
        if (op->name == vn_) {
            count++;
        } else if (scope_.contains(op->name)) {
            scope_.ref(op->name).accept(this);
        }
    }

    void visit(const Select* op) {
        if (visit_cond_) {
            op->condition.accept(this);
        }
        op->true_value.accept(this);
        op->false_value.accept(this);
    }

 public:
    int count;

    CountVariable(Scope<Expr>& scope, const std::string& vn, bool visit_cond)
        : scope_(scope), vn_(vn), visit_cond_(visit_cond), count(0)
    {}
};

int count_variable(Expr expr, const std::string& vn, bool visit_cond)
{
    Scope<Expr> scope;
    return count_variable(expr, scope, vn, visit_cond);
}

int count_variable(Expr expr, Scope<Expr>& scope, const std::string& vn, bool visit_cond)
{
    // TODO : memoization
    CountVariable v(scope, vn, visit_cond);
    expr.accept(&v);
    return v.count;
}

bool find_variable(Expr expr, const std::string& vn)
{
    return count_variable(expr, vn) > 0;
}

bool find_variable(Expr expr, Scope<Expr>& scope, const std::string& vn, bool visit_cond)
{
    return count_variable(expr, scope, vn, visit_cond) > 0;
}

} // Internal
} // Halide
