#include <cstdio>
#include <cstdint>

#include "Halide.h"

using namespace Halide;

Func vadd()
{
    Func a("a"), b("b"), c("c");
    Var i("i");
    a(i) = i;
    a.compute_root();
    b(i) = i + 3;
    b.compute_root();

    c(i) = a(i) + b(i);

    return c;
}

int main(int argc, char **argv) {

    constexpr int32_t size = 100;
    Buffer<int32_t> output_org(size), output_para(size), output_vec(size), output_poly(size);

    Func f_org = vadd();
    f_org.compile_to_c("vadd.c", {});

    Func f_para = vadd();
    Var i_para = f_para.args()[0];
    f_para.parallel(i_para);
    f_para.compile_to_c("vadd_para.c", {});

    Func f_vec = vadd();
    Var i_vec = f_vec.args()[0];
    f_vec.vectorize(i_vec, 8);
    f_vec.realize(output_vec);

    Func f_poly = vadd();
    Target target = get_host_target();
    target.set_feature(Target::ApplyPolyhedralModel);
    f_poly.compile_to_c("vadd_poly.c", {}, "", target);

    printf("Success!\n");
    return 0;
}
