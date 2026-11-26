// Keep the existing C checks exercising the Zig implementations through their C ABI.
comptime {
    _ = @import("game_modules");
}

extern fn ui_checks_main(argc: c_int, argv: [*c][*c]u8) c_int;

pub export fn main(argc: c_int, argv: [*c][*c]u8) c_int {
    return ui_checks_main(argc, argv);
}
