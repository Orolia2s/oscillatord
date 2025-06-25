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
        .files = &SOURCES,
        .flags = &CFLAGS,
    });
    lib.addIncludePath(b.path("include"));
    lib.addIncludePath(pps.path(""));
    lib.addIncludePath(logc.path("src"));
    lib.installHeadersDirectory(b.path("include"), "", .{});
    b.installArtifact(lib);

    // Executable
    const exe = b.addExecutable(.{ .name = "oscillatord", .target = target, .optimize = optimize });
    exe.linkLibC();
    exe.addCSourceFile(.{ .file = b.path("oscillatord.c"), .flags = &CFLAGS });
    exe.addCSourceFile(.{ .file = logc.path("src/log.c"), .flags = &CFLAGS });
    exe.addIncludePath(b.path("src"));
    exe.addIncludePath(logc.path("src"));
    exe.linkLibrary(lib);
    b.installArtifact(exe);

    for (deps) |dep| {
        lib.linkLibrary(dep);
        exe.linkLibrary(dep);
    }

    {
        // Run
        const run_step = b.step("run", "Run oscillatord with the default configuration");
        const run = b.addRunArtifact(exe);
        run.addFileArg(b.path("configuration/oscillatord_default.conf"));
        run_step.dependOn(&run.step);
    }

    { // Utils
        const step = b.step("utils", "Build the utilities");
        const utils: [UTILS.len][]const u8 = UTILS;

        for (utils) |util| {
            var name = b.dupe(util);
            name.len -= 2;
            const util_exe = b.addExecutable(.{ .name = name, .target = target, .optimize = optimize });
            util_exe.linkLibC();
            util_exe.linkLibrary(lib);
            util_exe.addIncludePath(b.path("src"));
            util_exe.addIncludePath(logc.path("src"));
            for (deps) |dep| {
                util_exe.linkLibrary(dep);
            }
            util_exe.addCSourceFiles(.{ .root = b.path("utils"), .files = &.{ util, "eeprom.c" }, .flags = &CFLAGS });
            util_exe.addCSourceFile(.{ .file = logc.path("src/log.c"), .flags = &CFLAGS });
            const install_util = b.addInstallArtifact(util_exe, .{});
            step.dependOn(&install_util.step);
        }
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

const SOURCES = .{
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
};

const UTILS = .{
    "art_disciplining_manager.c",
    "art_eeprom_files_updater.c",
    "art_eeprom_format.c",
    "art_eeprom_reformat.c",
    "art_monitoring_client.c",
    "art_temperature_table_manager.c",
};
