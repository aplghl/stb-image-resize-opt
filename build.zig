const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Exact, byte-identical output.
    const base_flags = [_][]const u8{ "-O3", "-ffp-contract=off", "-fvisibility=hidden" };
    const avx2_flags = [_][]const u8{ "-O3", "-ffp-contract=off", "-fvisibility=hidden" };
    const disp_flags = [_][]const u8{ "-O3", "-ffp-contract=off", "-DSTBIR_DISPATCH_AVX2" };
    const plain_flags = [_][]const u8{ "-O3", "-ffp-contract=off" };

    if (target.result.cpu.arch == .x86_64) {
        // Baseline (SSE2) and AVX2 (Haswell / x86-64-v3) are separate modules so
        // each gets the right CPU model; the library links them together. The
        // AVX2 object is selected at runtime by lib/resize_dispatch.c.
        var base_q = target.query;
        base_q.cpu_model = .baseline;
        const base_target = b.resolveTargetQuery(base_q);

        var avx2_q = target.query;
        avx2_q.cpu_model = .{ .explicit = &std.Target.x86.cpu.haswell };
        const avx2_target = b.resolveTargetQuery(avx2_q);

        const lib_mod = b.createModule(.{ .target = base_target, .optimize = optimize, .link_libc = true });
        lib_mod.addIncludePath(b.path("src"));
        lib_mod.addCSourceFile(.{ .file = b.path("lib/resize_base.c"), .flags = &base_flags });
        lib_mod.addCSourceFile(.{ .file = b.path("lib/resize_dispatch.c"), .flags = &disp_flags });

        const avx2_mod = b.createModule(.{ .target = avx2_target, .optimize = optimize, .link_libc = true });
        avx2_mod.addIncludePath(b.path("src"));
        avx2_mod.addCSourceFile(.{ .file = b.path("lib/resize_avx2.c"), .flags = &avx2_flags });
        const avx2_obj = b.addObject(.{ .name = "resize_avx2", .root_module = avx2_mod });

        lib_mod.addObject(avx2_obj);

        const lib = b.addLibrary(.{
            .name = "stb-image-resize-opt",
            .root_module = lib_mod,
            .linkage = .static,
        });
        b.installArtifact(lib);
    } else {
        const lib_mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true });
        lib_mod.addIncludePath(b.path("src"));
        lib_mod.addCSourceFile(.{ .file = b.path("lib/stb_image_resize2.c"), .flags = &plain_flags });
        const lib = b.addLibrary(.{
            .name = "stb-image-resize-opt",
            .root_module = lib_mod,
            .linkage = .static,
        });
        b.installArtifact(lib);
    }

    b.installFile("src/stb_image_resize2.h", "include/stb_image_resize2.h");
}
