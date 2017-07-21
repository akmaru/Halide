#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "FindVariable.h"
#include "Polytope.h"
#include "IR.h"
#include "IREquality.h"
#include "IRPrinter.h"
#include "Simplify.h"
#include "Substitute.h"

namespace Halide {
namespace Internal {

std::ostream& operator<<(std::ostream& stream, const Domain& domain)
{
    stream << "[" << domain.min << ", " << domain.max << "]";
    return stream;
}


bool PolytopeDomain::include(const std::string& loopvar) const
{
    for (size_t i=0; i<domain.size(); i++) {
        if (domain[i].var == loopvar) {
            return true;
        }
    }

    return false;
}

void PolytopeDomain::update_for(const For *op)
{
    Expr max = simplify(op->min + op->extent - 1);
    domain.push_back({op->name, op->min, max});
}

void PolytopeDomain::downdate_for(const For *op)
{
    internal_assert(domain.size() > 0);

    domain.pop_back();
}

std::ostream& operator<<(std::ostream& stream, const PolytopeDomain& domain)
{
    for (size_t i=0; i<domain.size()-1; i++) {
        stream << domain.domain[i] << ", ";
    }
    if (domain.size() > 0) {
        stream << domain.domain[domain.size()-1];
    }

    return stream;
}


size_t PolytopeSchedule::get_index(const std::string& var) const
{
    for (size_t i=0; i<schedule.size(); i++) {
        const Variable* v = schedule[i].as<Variable>();

        if (v != nullptr && v->name == var) {
            return i;
        }
    }

    return -1;
}

void PolytopeSchedule::update_stmt()
{
    internal_assert(schedule.size() > 0);

    schedule.back() = simplify(schedule.back() + 1);

}

void PolytopeSchedule::update_for(const For *op)
{
    internal_assert(schedule.size() > 0);

    Expr max = simplify(op->min + op->extent - 1);
    schedule.push_back(Variable::make(Int(32), op->name));
    schedule.push_back(0);
}

void PolytopeSchedule::downdate_for(const For *op)
{
    const IntImm *i = schedule.back().as<IntImm>();
    internal_assert(i != nullptr) << "Unsupported pattern";
    schedule.pop_back();

    const Variable *v = schedule.back().as<Variable>();
    internal_assert(v != nullptr && v->name == op->name) << "Unsupported pattern";
    schedule.pop_back();

    schedule.back() = simplify(schedule.back() + 1);
}

std::ostream& operator<<(std::ostream& stream, const PolytopeSchedule& schedule)
{
    IRPrinter p(stream);

    stream << "(";
    p.print_list(schedule.schedule);
    stream << ")";

    return stream;
}


FuncPoly::FuncPoly(const std::string& name_, FuncPoly::Type type_, const std::vector<Expr>& args_,
                   const PolytopeDomain& domain_, const PolytopeSchedule& schedule_)
    : name(name_), type(type_), args(args_), domain(domain_), schedule(schedule_), args_are_linear(true)
{
    check_args();
}

bool FuncPoly::overlapped() const
{
    for (size_t i=0; i<domain.size(); i++) {
        std::string loop_var = domain.domain[i].var;
        bool found = false;

        for (size_t j=0; j<schedule.size(); j++) {
            if (find_variable(schedule.schedule[j], loop_var)) {
                found = true;
                break;
            }
        }

        if (!found) {
            return true;
        }
    }

    return true;
}


void FuncPoly::DeriveVars::visit(const Variable* op)
{
    auto it = std::find_if(domain_.begin(), domain_.end(),
                        [&op](const Domain& d) { return op->name == d.var; });

    if (it != domain_.end()) {
        founds.push_back(op->name);
    }
}

FuncPoly::DeriveVars::DeriveVars(const PolytopeDomain& domain)
    : domain_(domain.domain)
{}


void FuncPoly::check_args()
{
    arg_loopvars.resize(args.size());
    arg_items.resize(args.size());
    arg_rests.resize(args.size());
    std::vector<std::set<std::string> > derives(args.size());

    for (size_t i=0; i<args.size(); i++) {
        arg_loopvars[i] = "";
        arg_items[i] = 0;
        arg_rests[i] = args[i];

        DeriveVars v(domain);
        args[i].accept(&v);

        if (v.founds.size() == 0) {
            args_are_linear &= true;
        } else if (v.founds.size() == 1) {
            args_are_linear &= true;
            arg_loopvars[i] = v.founds.front();
            arg_items[i] = Variable::make(Int(32), v.founds.front());
            arg_rests[i] = substitute(v.founds.front(), 0, args[i]);
        }
    }
}


StmtPoly::StmtPoly(const PolytopeDomain& domain_, const PolytopeSchedule& schedule_,
                   const std::vector<std::shared_ptr<FuncPoly> >& provides_,
                   const std::vector<std::shared_ptr<FuncPoly> >& calls_)
    : domain(domain_), schedule(schedule_), provides(provides_), calls(calls_)
{}

std::string StmtPoly::indents() const
{
    std::string indent = "";
    for (size_t i=0; i<domain.size(); i++) {
        indent += "  ";
    }

    return indent;
}

std::ostream& operator<<(std::ostream& stream, const StmtPoly& stmt)
{
    IRPrinter p(stream);

    // Iteration sets
    stream << stmt.indents() << "  Iteration Sets := (";
    for (size_t i=0; i<stmt.domain.size()-1; i++) {
        stream << stmt.domain.domain[i].var << ", ";
    }
    if (stmt.domain.size() > 0) {
        stream << stmt.domain.domain[stmt.domain.size()-1].var;
    }
    stream << ")\n";

    // Domain
    stream << stmt.indents() << "  Domain := " << stmt.domain << "\n";

    // Schedule
    stream << stmt.indents() << "  Schedule := " << stmt.schedule << "\n";

    // Provides
    if (stmt.provides.size() > 0) {
        stream << stmt.indents() << "  Provides :=\n";
        for (size_t i=0; i<stmt.provides.size(); i++) {
            const auto& provide = stmt.provides[i];
            stream << stmt.indents() << "    " << provide->name << " := (";
            p.print_list(provide->args);
            stream << ") :(";
            p.print_list(provide->arg_items);
            stream << ")\n";
        }
    }

    // Calls
    if (stmt.calls.size() > 0) {
        stream << stmt.indents() << "  Calls :=\n";
        for (size_t i=0; i<stmt.calls.size(); i++) {
            const auto& call = stmt.calls[i];
            stream << stmt.indents() << "    " << call->name << " := (";
            p.print_list(call->args);
            stream << ") : ( ";
            p.print_list(call->arg_items);
            stream << ")\n";
        }
    }

    stream << "\n";

    return stream;
}

std::vector<Expr> compute_distances(const std::vector<Expr> args_a, const std::vector<Expr> args_b)
{
    internal_assert(args_a.size() == args_b.size()) << "Args sizes should be equal.\n";
    std::vector<Expr> distances(args_a.size());

    for (size_t i=0; i<args_a.size(); i++) {
        distances[i] = args_b[i] - args_a[i];
    }

    return distances;
}

DependencyPolyhedra::Direction DependencyPolyhedra::compute_direction(Expr a, Expr b)
{
    Direction dir = Direction::Unknown;

    const int64_t* diff = as_const_int(simplify(b - a));

    if (diff == nullptr) {
        dir = Direction::Unknown;
    } else {
        if (*diff > 0) {
            dir = Direction::Less;
        } else if (*diff < 0) {
            dir = Direction::Greater;
        } else {
            dir = Direction::Equal;
        }
    }

    return dir;
}


void DependencyPolyhedra::compute_directions()
{
    const auto& a = replaced_schedule;
    const auto& b = target->schedule;

    direction = Direction::Equal;
    const size_t common_size = std::min(a.size(), b.size());
    directions.resize(common_size);

    for (size_t i=0; i<common_size; i++) {
        Direction d = compute_direction(a.schedule[i], b.schedule[i]);
        directions[i] = d;

        if (direction == Direction::Equal) {
            direction = d;
        }
    }


    if (direction == Direction::Equal) {
        internal_assert(a.size() == b.size()) << "Two different size schedules are should not be equal.\n";

        // If accesses are overlaped, exist anti dependency.
        if (source->overlapped() || target->overlapped()) {
            direction = Direction::Greater;
        }
    }
}


void DependencyPolyhedra::compute_iter_replacement()
{
    std::map<std::string, Expr> replacements;
    for (size_t i=0; i<source->args.size(); i++) {
        iter_replacement[i] = simplify(target->args[i] - source->arg_rests[i]);
        replacements[source->arg_loopvars[i]] = iter_replacement[i];
    }

    for (size_t i=0; i<replaced_schedule.size(); i++) {
        replaced_schedule.schedule[i] = substitute(replacements,
                                                   replaced_schedule.schedule[i]);
    }
}

void DependencyPolyhedra::fix_source_target()
{
    if (direction == Direction::Greater) {
        // Swap source and target.
        auto tmp = source;
        source = target;
        target = tmp;

        for (auto& d : directions) {
            if (d == Direction::Less) {
                d = Direction::Greater;
            } else if (d == Direction::Greater) {
                d = Direction::Less;
            }
        }
    } else if(direction == Direction::Equal &&
              source->type == FuncPoly::Type::Provide && target->type == FuncPoly::Type::Call) {
        // Swap source and target.
        auto tmp = source;
        source = target;
        target = tmp;
    }
}

void DependencyPolyhedra::detect_kind()
{
    if (direction == Direction::Unknown) {
        kind = Kind::Unknown;
    } else if (direction == Direction::Equal) {
        kind = Kind::None;
    } else if (source->type == FuncPoly::Type::Provide && target->type == FuncPoly::Type::Call) {
        kind = Kind::Flow;
    } else if (source->type == FuncPoly::Type::Call && target->type == FuncPoly::Type::Provide) {
        kind = Kind::Anti;
    } else if (source->type == FuncPoly::Type::Provide && target->type == FuncPoly::Type::Provide) {
        kind = Kind::Output;
    }
}

DependencyPolyhedra::DependencyPolyhedra(const std::shared_ptr<FuncPoly>& source_, const std::shared_ptr<FuncPoly>& target_)
    : source(source_), target(target_)
{
    if (!source->args_are_linear || !target->args_are_linear) {
        direction = Direction::Unknown;
        return;
    }

    iter_replacement.resize(source->args.size());
    replaced_schedule = source->schedule;

    compute_iter_replacement();
    compute_directions();
    fix_source_target();
    detect_kind();
}

std::ostream& operator<<(std::ostream& stream, const DependencyPolyhedra& dep)
{
    std::string kind_str;
    IRPrinter p(stream);

    switch(dep.kind) {
    case DependencyPolyhedra::Kind::Flow:
        kind_str = "Flow: ";
        break;
    case DependencyPolyhedra::Kind::Anti:
        kind_str = "Anti: ";
        break;
    case DependencyPolyhedra::Kind::Output:
        kind_str = "Output: ";
        break;
    case DependencyPolyhedra::Kind::Unknown:
        kind_str = "Unknown: ";
        break;
    default:
        ;
    }

    stream << kind_str;
    stream << dep.source->name << "(";
    p.print_list(dep.source->args);
    stream << ") -> " << dep.target->name << "(";
    p.print_list(dep.target->args);
    stream << ") : (";

    for (size_t i=0; i<dep.directions.size(); i++) {
        std::string dir_str;
        switch(dep.directions[i]) {
        case DependencyPolyhedra::Direction::Equal:
            dir_str = "=,";
            break;
        case DependencyPolyhedra::Direction::Less:
            dir_str = "-";
            break;
        case DependencyPolyhedra::Direction::Greater:
            dir_str = "+";
            break;
        case DependencyPolyhedra::Direction::Unknown:
            dir_str = "*";
            break;
        default:
            ;
        }

        stream << dir_str << ", ";
    }

    stream << ")\n";

    return stream;
}



Polytope::Builder::Builder(std::vector<std::shared_ptr<FuncPoly> >& funcs,
                           std::vector<std::shared_ptr<StmtPoly> >& stmts)
    : funcs_(funcs), stmts_(stmts)
{
    in_scop_.push(false);
}

void Polytope::Builder::visit(const Realize *op)
{
    IRVisitor::visit(op);
}

void Polytope::Builder::visit(const ProducerConsumer *op)
{
    in_scop_.push(true);
    realizes_.insert(op->name);
    IRVisitor::visit(op);
    in_scop_.pop();
}

void Polytope::Builder::visit(const For *op)
{
    if (!in_scop_.top()) {
        IRVisitor::visit(op);
        return;
    }

    domain_.update_for(op);
    schedule_.update_for(op);
    IRVisitor::visit(op);
    domain_.downdate_for(op);
    schedule_.downdate_for(op);
}

void Polytope::Builder::visit(const LetStmt *op)
{
    if (!in_scop_.top()) {
        IRVisitor::visit(op);
        return;
    }

    op->value.accept(this);

    internal_assert(provides_.empty()) << "Provides should not be appeared in Let\n";

    if (!calls_.empty()) {
        stmts_.push_back(std::make_shared<StmtPoly>(domain_, schedule_, provides_, calls_));
        schedule_.update_stmt();
        calls_.clear();
    }

    op->body.accept(this);
}

void Polytope::Builder::visit(const Provide *op)
{
    IRVisitor::visit(op);

    if (!in_scop_.top()) {
        return;
    }

    if (realizes_.count(op->name) > 0) {
        auto provide = std::make_shared<FuncPoly>(op->name, FuncPoly::Type::Provide, op->args, domain_, schedule_);
        funcs_.push_back(provide);
        provides_.push_back(provide);
    }

    if (!calls_.empty() || !provides_.empty()) {
        stmts_.push_back(std::make_shared<StmtPoly>(domain_, schedule_, provides_, calls_));
        schedule_.update_stmt();
        provides_.clear();
        calls_.clear();
    }
}

void Polytope::Builder::visit(const Call *op)
{
    IRVisitor::visit(op);

    if (!in_scop_.top()) {
        return;
    }

    if (realizes_.count(op->name) > 0) {
        auto call = std::make_shared<FuncPoly>(op->name, FuncPoly::Type::Call, op->args, domain_, schedule_);
        funcs_.push_back(call);
        calls_.push_back(call);
    }
}


void Polytope::compute_polytope(Stmt s)
{
    Builder v(funcs_, stmts_);
    s.accept(&v);
}


void Polytope::compute_dependency()
{
    for (size_t i=0; i<funcs_.size(); i++) {
        auto a = funcs_[i];
        if (a->type == FuncPoly::Type::Call) {
            continue;
        }

        for (size_t j=0; j<funcs_.size(); j++) {
            if (i == j) {
                continue;
            }

            auto b = funcs_[j];
            if (a->name == b->name) {
                if (b->type == FuncPoly::Type::Provide && i > j) {
                    continue;
                }

                deps_.push_back(std::make_shared<DependencyPolyhedra>(a, b));
            }
        }
    }
}

std::vector<std::shared_ptr<DependencyPolyhedra> > Polytope::get_dependencies(const std::string& loopvar) const
{
    std::vector<std::shared_ptr<DependencyPolyhedra> > rets;

    for (const auto& dep : deps_) {
        if (dep->source->domain.include(loopvar) && dep->target->domain.include(loopvar)) {
            rets.push_back(dep);
        }
    }

    return rets;
}


std::ostream& operator<<(std::ostream& stream, const Polytope& poly)
{
    for (const auto& stmt : poly.stmts_) {
        stream << *stmt;
    }

    for (const auto& dep : poly.deps_) {
        stream << *dep;
    }

    return stream;
}

}
}
