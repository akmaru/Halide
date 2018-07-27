#include <cstdio>
#include <cstdint>

#include "Halide.h"

using namespace Halide;

Func vadd()
{
    Func a("a"), b("b"), c("c");
    Var i("i"), j("j");
    a(i, j) = i + j;
    a.compute_root();
    b(i, j) = i + j + 3;
    b.compute_root();

    c(i, j) = a(i, j) + b(i, j);

    return c;
}

int main(int argc, char **argv) {

    constexpr int32_t size = 100;
    Buffer<int32_t> output_org(size, size), output_para(size, size), output_vec(size, size), output_poly(size, size);

    Func f_org = vadd();
    f_org.compile_to_c("vadd2d.c", {});

    Func f_para = vadd();
    Var i_para = f_para.args()[0];
    f_para.parallel(i_para);
    f_para.compile_to_c("vadd2d_para.c", {});

    Func f_vec = vadd();
    Var i_vec = f_vec.args()[0];
    f_vec.vectorize(i_vec, 8);
    f_vec.realize(output_vec);

    Func f_poly = vadd();
    Target target = get_host_target();
    target.set_feature(Target::ApplyPolyhedralModel);
    f_poly.compile_to_c("vadd2d_poly.c", {}, "", target);

    printf("Success!\n");
    return 0;
}
