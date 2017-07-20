#include <cstdio>
#include <cstdint>

#include "Halide.h"

using namespace Halide;

Func fibonacci(int32_t size)
{
        Var x("x");
        RDom r(2, size-2);
        Func f("f");

        f(x) = x;
        f(r.x) = f(r.x-2) + f(r.x-1);

        return f;
}

int main(int argc, char **argv) {

    constexpr int32_t size = 100;
    Buffer<int32_t> output(size);

    Func f = fibonacci(size);
    Target target = get_host_target();
    target.set_feature(Target::ApplyPolyhedralModel);
    f.realize(output, target);

    printf("Success!\n");
    return 0;
}
