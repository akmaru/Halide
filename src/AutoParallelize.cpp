#include <vector>
#include <memory>

#include "Debug.h"
#include "IRMutator.h"
#include "Polytope.h"

namespace Halide {
namespace Internal {

namespace {

class AutoParallelize : public IRMutator {
    using IRMutator::visit;

    const Polytope& polytope_;

    bool can_parallelize(const std::string& loopvar) {
        bool ret = true;
        std::vector<std::shared_ptr<DependencyPolyhedra> > deps = polytope_.get_dependencies(loopvar);

        for (const auto& dep : deps) {
            size_t source_index = dep->source->schedule.get_index(loopvar);
            size_t target_index = dep->target->schedule.get_index(loopvar);

            internal_assert(source_index == target_index) << "fixme";

            if (dep->directions[source_index] != DependencyPolyhedra::Direction::Equal) {
                ret = false;
            }
        }

        if (ret) {
            debug(2) << "Loop: " << loopvar << " can be parallelized\n";
        } else {
            debug(2) << "Loop: " << loopvar << "cannot be parallelized\n";
        }

        return ret;
    }

    void visit(const For* op) {
        bool parallelizable = can_parallelize(op->name);

        if (parallelizable) {
            stmt = For::make(op->name, op->min, op->extent, ForType::Parallel, op->device_api, op->body);
        } else {
            Stmt body = mutate(op->body);
            if (body.same_as(op->body)) {
                stmt = op;
            } else {
                stmt = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
            }
        }
    }

public:
    AutoParallelize(const Polytope& polytope)
        : polytope_(polytope)
    {}
};

} // Anonymous


Stmt auto_parallelize(Stmt s, const Polytope& polytope)
{
    return AutoParallelize(polytope).mutate(s);
}

}
}
