const std = @import("std");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseSafe });
    const target = b.standardTargetOptions(.{});

    const jsonc = b.dependency("jsonc", .{ .target = target });
    const minipod = b.dependency("disciplining_minipod", .{ .target = target });
    const ubloxcfg = b.dependency("ubloxcfg", .{ .target = target });
    const pps = b.dependency("pps_tools", .{});

    const deps = [_]*std.Build.Step.Compile{
        jsonc.artifact("json-c"),
        minipod.artifact("disciplining_minipod"),
        ubloxcfg.artifact("ubloxcfg"),
    };

    const lib = b.addStaticLibrary(.{
        .name = "oscillatord",
        .target = target,
        .optimize = optimize,
    });
    lib.addCSourceFiles(.{
        .root = b.path("src"),
        .files = &.{
            "ntpshm/ppsthread.c",
            "ntpshm/timehint.c",
            "ntpshm/timespec_str.c",
            "ntpshm/ntpshmread.c",
            "ntpshm/ntpshmwrite.c",
            "monitoring.c",
            "phasemeter.c",
            "oscillator.c",
            "gnss.c",
            "oscillator_factory.c",
            "common/config.c",
            "common/f9_defvalsets.c",
            "common/utils.c",
            "common/eeprom_config.c",
            "common/gnss-config.c",
            "common/log.c",
            "oscillators/dummy_oscillator.c",
            "oscillators/sim_oscillator.c",
            "oscillators/sa3x_oscillator.c",
            "oscillators/mRo50_oscillator.c",
            "oscillators/sa5x_oscillator.c",
        },
        .flags = &CFLAGS,
    });
    lib.addIncludePath(b.path("include"));
    lib.addIncludePath(pps.path(""));
    lib.installHeadersDirectory(b.path("include"), "", .{});
    lib.linkLibC();
    b.installArtifact(lib);

    const exe = b.addExecutable(.{ .name = "oscillatord", .target = target, .optimize = optimize });
    exe.addCSourceFile(.{ .file = b.path("oscillatord.c"), .flags = &CFLAGS });
    exe.addIncludePath(b.path("src"));
    exe.addIncludePath(b.path("include"));
    exe.linkLibrary(lib);
    b.installArtifact(exe);

    for (deps) |dep| {
        lib.linkLibrary(dep);
        exe.linkLibrary(dep);
    }
}

const CFLAGS = .{
    "-Wall",
    "-Wextra",
    "-Wmissing-prototypes",
    "-Wmissing-declarations",
    "-Wformat=2",
    "-Wold-style-definition",
    "-Wstrict-prototypes",
    "-Wpointer-arith",
    "-Wno-address-of-packed-member",
};
