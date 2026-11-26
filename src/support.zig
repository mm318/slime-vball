const std = @import("std");

pub fn text(buffer: []u8, comptime format: []const u8, args: anytype) [:0]const u8 {
    return std.mem.printSentinel(buffer, format, args, 0) catch unreachable;
}

pub fn index(value: c_int) usize {
    return @intCast(value);
}
