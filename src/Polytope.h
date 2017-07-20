#ifndef HALIDE_POLYTOPE_H
#define HALIDE_POLYTOPE_H

#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "IR.h"
#include "IRVisitor.h"
#include "Expr.h"
#include "Util.h"
#include "Schedule.h"

namespace Halide {
namespace Internal {

struct Domain {
    std::string var;
    Expr min;
    Expr max;
};

std::ostream& operator<<(std::ostream& stream, const Domain& domain);


struct PolytopeDomain {
    std::vector<Domain> domain;

    inline size_t size() const { return domain.size(); }
    bool include(const std::string& loopvar) const;

    void update_for(const For *op);
    void downdate_for(const For *op);
};

std::ostream& operator<<(std::ostream& stream, const PolytopeDomain& domain);


struct PolytopeSchedule {
    std::vector<Expr> schedule = {0};

    inline size_t size() const { return schedule.size(); }
    size_t get_index(const std::string& var) const;

    void update_stmt();
    void update_for(const For *op);
    void downdate_for(const For *op);
    bool operator<(const PolytopeSchedule& rhs);
};

std::ostream& operator<<(std::ostream& stream, const PolytopeSchedule& schedule);


struct FuncPoly {
    enum class Type {
        Provide,
        Call
    };

    std::string name;
    Type type;
    std::vector<Expr> args;
    const PolytopeDomain domain;
    const PolytopeSchedule schedule;

    bool args_are_linear;
    std::vector<std::string> arg_loopvars;
    std::vector<Expr> arg_items;
    std::vector<Expr> arg_rests;

    FuncPoly(const std::string& name_, Type type_, const std::vector<Expr>& args_,
             const PolytopeDomain& domain_, const PolytopeSchedule& schedule_);
    bool overlapped() const;

private:
    class DeriveVars : public IRVisitor {
        using IRVisitor::visit;

        const std::vector<Domain>& domain_;

        void visit(const Variable* op);

    public:
        std::vector<std::string> founds;

        DeriveVars(const PolytopeDomain& domain);
    };

    void check_args();
};


struct StmtPoly {
    const PolytopeDomain domain;
    const PolytopeSchedule schedule;
    std::vector<std::shared_ptr<FuncPoly> > provides;
    std::vector<std::shared_ptr<FuncPoly> > calls;

    StmtPoly(const PolytopeDomain& domain_, const PolytopeSchedule& schedule_,
             const std::vector<std::shared_ptr<FuncPoly> >& provides_,
             const std::vector<std::shared_ptr<FuncPoly> >& calls_);

    std::string indents() const;
    friend std::ostream& operator<<(std::ostream& stream, const StmtPoly& stmt);
};


struct DependencyPolyhedra {
    enum class Direction {
        Equal,
        Less,
        Greater,
        Unknown
    };

    enum class Kind {
        Flow,
        Anti,
        Output,
        None,
        Unknown
    };

    std::shared_ptr<FuncPoly> source;
    std::shared_ptr<FuncPoly> target;

    std::vector<Expr> iter_replacement;
    PolytopeSchedule replaced_schedule;
    Direction direction;
    std::vector<Direction> directions;
    Kind kind;

    DependencyPolyhedra(const std::shared_ptr<FuncPoly>& a, const std::shared_ptr<FuncPoly>& b);

    friend std::ostream& operator<<(std::ostream& stream, const DependencyPolyhedra& dep);

private:
    void compute_iter_replacement();
    Direction compute_direction(Expr a, Expr b);
    void compute_directions();
    void fix_source_target();
    void detect_kind();
};


class Polytope {
public:
    void analyze();

private:
    std::vector<std::shared_ptr<FuncPoly> > funcs_;
    std::vector<std::shared_ptr<StmtPoly> > stmts_;
    std::vector<std::shared_ptr<DependencyPolyhedra> > deps_;

    class Builder : public IRVisitor {
        using IRVisitor::visit;

        std::vector<std::shared_ptr<FuncPoly> >& funcs_;
        std::vector<std::shared_ptr<StmtPoly> >& stmts_;

        std::stack<bool> in_scop_;
        std::set<std::string> realizes_;
        PolytopeDomain domain_;
        PolytopeSchedule schedule_;
        std::vector<std::shared_ptr<FuncPoly> > provides_;
        std::vector<std::shared_ptr<FuncPoly> > calls_;

        void visit(const Realize *op);
        void visit(const ProducerConsumer *op);
        void visit(const For *op);
        void visit(const LetStmt *op);
        void visit(const Provide *op);
        void visit(const Call *op);

    public:
        Builder(std::vector<std::shared_ptr<FuncPoly> >& funcs, std::vector<std::shared_ptr<StmtPoly> >& stmts);
    };

public:
    void compute_polytope(Stmt s);
    void compute_dependency();
    std::vector<std::shared_ptr<DependencyPolyhedra> > get_dependencies(const std::string& loopvar) const;
    friend std::ostream& operator<<(std::ostream& stream, const Polytope& c);
};


}
}

#endif
