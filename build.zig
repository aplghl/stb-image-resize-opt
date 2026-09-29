const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Exact, byte-identical output.
    const base_flags = [_][]const u8{ "-O3", "-ffp-contract=off", "-march=x86-64-v2", "-fvisibility=hidden" };
    const avx2_flags = [_][]const u8{ "-O3", "-ffp-contract=off", "-march=x86-64-v3", "-fvisibility=hidden" };
    const disp_flags = [_][]const u8{ "-O3", "-ffp-contract=off", "-march=x86-64-v2", "-DSTBIR_DISPATCH_AVX2" };
    const plain_flags = [_][]const u8{ "-O3", "-ffp-contract=off" };

    const lib_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib_mod.addIncludePath(b.path("src"));

    if (target.result.cpu.arch == .x86_64) {
        lib_mod.addCSourceFile(.{ .file = b.path("lib/resize_base.c"), .flags = &base_flags });
        lib_mod.addCSourceFile(.{ .file = b.path("lib/resize_avx2.c"), .flags = &avx2_flags });
        lib_mod.addCSourceFile(.{ .file = b.path("lib/resize_dispatch.c"), .flags = &disp_flags });
    } else {
        lib_mod.addCSourceFile(.{ .file = b.path("lib/stb_image_resize2.c"), .flags = &plain_flags });
    }

    const lib = b.addLibrary(.{
        .name = "stb-image-resize-opt",
        .root_module = lib_mod,
        .linkage = .static,
    });
    b.installArtifact(lib);
    b.installFile("src/stb_image_resize2.h", "include/stb_image_resize2.h");
}
