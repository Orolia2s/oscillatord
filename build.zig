const std = @import("std");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseSafe });
    const target = b.standardTargetOptions(.{});

    const jsonc = b.dependency("jsonc", .{ .target = target, .optimize = optimize });
    const minipod = b.dependency("disciplining_minipod", .{ .target = target });
    const ubloxcfg = b.dependency("ubloxcfg", .{ .target = target });
    const pps = b.dependency("pps_tools", .{});
    const logc = b.dependency("logc", .{});

    const deps = [_]*std.Build.Step.Compile{
        jsonc.artifact("json-c"),
        minipod.artifact("disciplining_minipod"),
        ubloxcfg.artifact("ubloxcfg"),
    };

    const mod = b.addModule("oscillatord", .{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    mod.addCMacro("PACKAGE_VERSION", "3.8.3");
    mod.addCMacro("LOG_USE_COLOR", "1");

    const lib = b.addStaticLibrary(.{
        .name = "oscillator",
        .root_module = mod,
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
            "common/log.c",
            "common/config.c",
            "common/f9_defvalsets.c",
            "common/utils.c",
            "common/eeprom_config.c",
            "common/gnss-config.c",
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
    lib.addIncludePath(logc.path("src"));
    lib.installHeadersDirectory(b.path("include"), "", .{});
    for (deps) |dep| {
        lib.linkLibrary(dep);
    }
    b.installArtifact(lib);

    {
        // Executable
        const exe = b.addExecutable(.{ .name = "oscillatord", .root_module = mod });
        exe.addCSourceFile(.{ .file = b.path("oscillatord.c"), .flags = &CFLAGS });
        exe.addCSourceFile(.{ .file = logc.path("src/log.c"), .flags = &CFLAGS });
        exe.addIncludePath(b.path("src"));
        b.installArtifact(exe);

        // Run
        const run_step = b.step("run", "Run oscillatord with the default configuration");
        const run = b.addRunArtifact(exe);
        run.addFileArg(b.path("configuration/oscillatord_default.conf"));
        run_step.dependOn(&run.step);
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
    "-fno-sanitize=undefined",
};
