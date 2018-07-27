#include <cstdio>
#include <cstdint>

#include "Halide.h"

using namespace Halide;

Func matmul(int32_t size)
{
    Func a("a"), b("b"), c("c");
    Var i("i"), j("j");
    a(i, j) = i + j;
    a.compute_root();
    b(i, j) = i + j + 3;
    b.compute_root();
    RDom k(0, size);

    c(i, j) = 0;
    c(i, j) += a(k, j) * b(i, k);

    return c;
}

int main(int argc, char **argv) {

    constexpr int32_t size = 100;
    Buffer<int32_t> output_org(size, size), output_para(size, size), output_vec(size, size), output_poly(size, size);

    Func f_org = matmul(size);
    f_org.compile_to_c("matmul.c", {});

    Func f_para = matmul(size);
    Var i_para = f_para.args()[0];
    f_para.parallel(i_para);
    f_para.compile_to_c("matmul_para.c", {});

    Func f_vec = matmul(size);
    Var i_vec = f_vec.args()[0];
    f_vec.vectorize(i_vec, 8);
    f_vec.realize(output_vec);

    Func f_poly = matmul(size);
    Target target = get_host_target();
    target.set_feature(Target::ApplyPolyhedralModel);
    f_poly.compile_to_c("matmul_poly.c", {}, "", target);

    printf("Success!\n");
    return 0;
}
