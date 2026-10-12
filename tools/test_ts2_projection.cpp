#include <array>
#include <cmath>
#include <cstdio>
#include "hle/rt64_ts2_projection.h"

using Matrix = std::array<std::array<float, 4>, 4>;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
        if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); ++failures; }
    };
    // Bedroom capture: the detail projection is near 32 / far 6407;
    // the coarse background uses near 1100 / far 12002.
    for (auto range : {std::array<float, 2>{32.003f, 6406.88f}, {1100.104f, 12001.507f}}) {
        for (float scale : {1.0f, 0.5f, 0.125f}) {
            const float n = range[0], f = range[1];
            Matrix p{};
            p[0][0] = 1.5f * scale; p[1][1] = 2.0f * scale;
            p[2][2] = -(f + n) / (f - n) * scale;
            p[2][3] = -scale; p[3][2] = -2.0f * f * n / (f - n) * scale;
            const Matrix original = p;
            check(RT64::ts2RemovePerspectiveFarPlane(p), "finite perspective recognized");
            auto depth = [&](float distance) { return (-distance * p[2][2] + p[3][2]) / (-distance * p[2][3]); };
            check(std::abs(depth(n) + 1.0f) < 1e-5f, "near plane preserved");
            check(depth(n * 0.5f) < -1.0f, "near geometry still clipped");
            for (float d : {f, 20000.0f, 1000000.0f})
                check(depth(d) < 1.0f && depth(d) > -1.0f, "distant geometry inside depth range");
            check(depth(n * 2.0f) < depth(f), "depth ordering preserved");
            for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
                if (!(c == 2 && r >= 2)) check(p[r][c] == original[r][c], "screen projection unchanged");
            check(!RT64::ts2RemovePerspectiveFarPlane(p), "infinite projection unchanged on repeat");
        }
    }
    Matrix ortho{};
    for (int i = 0; i < 4; ++i) ortho[i][i] = 1.0f;
    const Matrix before = ortho;
    check(!RT64::ts2RemovePerspectiveFarPlane(ortho) && ortho == before, "orthographic UI unchanged");
    Matrix invalid{};
    check(!RT64::ts2RemovePerspectiveFarPlane(invalid), "zero matrix unchanged");
    if (!failures) std::puts("PASS: perspective cutoff removed; near clipping, depth order and screen projection preserved.");
    return failures ? 1 : 0;
}
