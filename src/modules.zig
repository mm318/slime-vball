// Importing the modules makes their C ABI entry points available to main.c and tests.
comptime {
    _ = @import("game.zig");
    _ = @import("ai.zig");
    _ = @import("replay.zig");
    _ = @import("ui.zig");
    _ = @import("input.zig");
}
