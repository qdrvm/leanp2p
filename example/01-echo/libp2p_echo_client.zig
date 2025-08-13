// LibP2P Echo Client Example in Zig
// Connects to an echo server and sends a message, printing the echoed reply.
// Usage:
//   libp2p_echo_client \
//     "/ip4/127.0.0.1/udp/12345/quic-v1/p2p/<peer-id>" [message]

const std = @import("std");

const c = @cImport({
    @cInclude("signal.h");
    @cInclude("string.h");
    @cInclude("libp2p/c/libp2p_c.h");
});

// Global context for signal handling
var g_context: ?*c.libp2p_context_t = null;

// Simple state for the request/response
const ClientState = extern struct {
    message: [1024]u8 = undefined,
    read_buffer: [1024]u8 = undefined,
};

fn cstrOrNull(p: [*c]const u8) ?[]const u8 {
    if (p == null) return null;
    const z: [*:0]const u8 = @ptrCast(p);
    return std.mem.span(z);
}

// Forward declarations
export fn on_read_complete(stream: ?*c.libp2p_stream_t, data: [*c]const u8, size: usize, err_code: c.libp2p_error_t, user_data: ?*anyopaque) callconv(.C) void {
    const state_ptr: *ClientState = @ptrCast(@alignCast(user_data.?));

    if (err_code != c.LIBP2P_SUCCESS) {
        if (err_code != c.LIBP2P_ERROR_IO_ERROR) {
            const err_str = c.libp2p_error_string(err_code);
            if (cstrOrNull(err_str)) |s| std.debug.print("Read failed: {s}\n", .{s});
        }
        _ = c.libp2p_stream_close(stream);
        c.libp2p_context_stop(g_context);
        std.c.free(state_ptr);
        return;
    }

    std.debug.print("Received {d} bytes: ", .{size});
    if (size > 0) {
        const print_size = if (size < state_ptr.read_buffer.len) size else state_ptr.read_buffer.len;
        const slice = data[0..print_size];
        std.debug.print("{s}", .{slice});
    }
    std.debug.print("\n", .{});

    _ = c.libp2p_stream_close(stream);
    c.libp2p_context_stop(g_context);
    std.c.free(state_ptr);
}

export fn on_write_complete(stream: ?*c.libp2p_stream_t, bytes_written: usize, err_code: c.libp2p_error_t, user_data: ?*anyopaque) callconv(.C) void {
    const state_ptr: *ClientState = @ptrCast(@alignCast(user_data.?));

    if (err_code != c.LIBP2P_SUCCESS) {
        const err_str = c.libp2p_error_string(err_code);
        if (cstrOrNull(err_str)) |s| std.debug.print("Write failed: {s}\n", .{s});
        _ = c.libp2p_stream_close(stream);
        c.libp2p_context_stop(g_context);
        std.c.free(state_ptr);
        return;
    }

    std.debug.print("Sent {d} bytes, waiting for echo...\n", .{bytes_written});
    const read_res = c.libp2p_stream_read(stream, &state_ptr.read_buffer[0], state_ptr.read_buffer.len, on_read_complete, user_data);
    if (read_res != c.LIBP2P_SUCCESS) {
        const err_str = c.libp2p_error_string(read_res);
        if (cstrOrNull(err_str)) |s| std.debug.print("Failed to initiate read: {s}\n", .{s});
        _ = c.libp2p_stream_close(stream);
        c.libp2p_context_stop(g_context);
        std.c.free(state_ptr);
    }
}

export fn on_stream_open(stream: ?*c.libp2p_stream_t, user_data: ?*anyopaque) callconv(.C) void {
    if (stream == null) {
        std.debug.print("Failed to open stream to peer\n", .{});
        c.libp2p_context_stop(g_context);
        std.c.free(user_data);
        return;
    }

    const state_ptr: *ClientState = @ptrCast(@alignCast(user_data.?));

    // Find the null terminator to get the message length
    var msg_len: usize = 0;
    while (msg_len < state_ptr.message.len and state_ptr.message[msg_len] != 0) {
        msg_len += 1;
    }

    std.debug.print("Opened echo stream, sending message: {s}\n", .{state_ptr.message[0..msg_len]});
    const write_res = c.libp2p_stream_write(stream, &state_ptr.message[0], msg_len, on_write_complete, user_data);
    if (write_res != c.LIBP2P_SUCCESS) {
        const err_str = c.libp2p_error_string(write_res);
        if (cstrOrNull(err_str)) |s| std.debug.print("Failed to initiate write: {s}\n", .{s});
        _ = c.libp2p_stream_close(stream);
        c.libp2p_context_stop(g_context);
        std.c.free(state_ptr);
    }
}

export fn on_connected(host: ?*c.libp2p_host_t, peer_id: [*c]const u8, user_data: ?*anyopaque) callconv(.C) void {
    std.debug.print("Connected callback fired, opening echo stream...\n", .{});
    const stream_res = c.libp2p_host_new_stream(host, peer_id, "/echo/1.0.0", on_stream_open, user_data);
    if (stream_res != c.LIBP2P_SUCCESS) {
        const err_str = c.libp2p_error_string(stream_res);
        if (cstrOrNull(err_str)) |s| std.debug.print("Failed to request new stream: {s}\n", .{s});
        c.libp2p_context_stop(g_context);
        std.c.free(user_data);
    }
}

fn signalHandler(sig: c_int) callconv(.C) void {
    _ = sig;
    std.debug.print("\nStopping...\n", .{});
    if (g_context) |ctx| {
        c.libp2p_context_stop(ctx);
    }
}

// Extract base multiaddress and peer id from a string like
//   "/ip4/127.0.0.1/udp/12345/quic-v1/p2p/<peerid>"
// Returns 0 on success, -1 on failure.
fn splitMultiaddrPeer(full: []const u8, addr_out: []u8, peer_out: []u8) !void {
    const tag = "/p2p/";
    const pos = std.mem.indexOf(u8, full, tag) orelse return error.InvalidFormat;

    const addr_len = pos;
    const peer_start = pos + tag.len;
    const peer_len = full.len - peer_start;

    if (addr_len >= addr_out.len or peer_len >= peer_out.len) {
        return error.BufferTooSmall;
    }

    @memcpy(addr_out[0..addr_len], full[0..addr_len]);
    addr_out[addr_len] = 0; // null terminate

    @memcpy(peer_out[0..peer_len], full[peer_start..]);
    peer_out[peer_len] = 0; // null terminate
}

pub fn main() !void {
    // Set up signal handler
    _ = c.signal(c.SIGINT, signalHandler);
    _ = c.signal(c.SIGTERM, signalHandler);

    // Set logging level
    c.libp2p_set_log_level(c.LIBP2P_LOG_INFO);

    std.debug.print("LibP2P Echo Client (Zig via C API)\n", .{});
    std.debug.print("===================================\n", .{});

    // Parse command line arguments
    const args = try std.process.argsAlloc(std.heap.page_allocator);
    defer std.process.argsFree(std.heap.page_allocator, args);

    if (args.len < 2) {
        std.debug.print("Usage: {s} \"/ip4/127.0.0.1/udp/<port>/quic-v1/p2p/<peer-id>\" [message]\n", .{args[0]});
        return;
    }

    const connect_full = args[1];
    const message_arg = if (args.len >= 3) args[2] else "Hello from Zig";

    // Prepare message state
    const mem = std.c.malloc(@sizeOf(ClientState));
    if (mem == null) {
        std.debug.print("Out of memory\n", .{});
        return;
    }
    var state_ptr: *ClientState = @ptrCast(@alignCast(mem.?));
    state_ptr.* = ClientState{};

    // Copy message into state
    const msg_len = @min(message_arg.len, state_ptr.message.len - 1);
    @memcpy(state_ptr.message[0..msg_len], message_arg[0..msg_len]);
    state_ptr.message[msg_len] = 0; // null terminate

    // Create context and host
    g_context = c.libp2p_context_create();
    if (g_context == null) {
        std.debug.print("Failed to create context\n", .{});
        std.c.free(state_ptr);
        return;
    }

    const keypair_seed: [*:0]const u8 = "echo_client_seed_0";
    const host = c.libp2p_host_create(g_context.?, keypair_seed);
    if (host == null) {
        std.debug.print("Failed to create host\n", .{});
        c.libp2p_context_destroy(g_context.?);
        std.c.free(state_ptr);
        return;
    }

    if (c.libp2p_host_start(host) != c.LIBP2P_SUCCESS) {
        std.debug.print("Failed to start host\n", .{});
        c.libp2p_host_destroy(host);
        c.libp2p_context_destroy(g_context.?);
        std.c.free(state_ptr);
        return;
    }

    // Split multiaddr and peer ID
    var base_addr: [512]u8 = undefined;
    var peer_id: [256]u8 = undefined;

    splitMultiaddrPeer(connect_full, &base_addr, &peer_id) catch {
        std.debug.print("Invalid connect address, expected .../p2p/<peer-id>\n", .{});
        c.libp2p_host_destroy(host);
        c.libp2p_context_destroy(g_context.?);
        std.c.free(state_ptr);
        return;
    };

    // Find null terminators to create proper C strings
    const base_addr_len = std.mem.indexOf(u8, &base_addr, &[_]u8{0}) orelse base_addr.len;
    const peer_id_len = std.mem.indexOf(u8, &peer_id, &[_]u8{0}) orelse peer_id.len;

    std.debug.print("Dialing {s} (peer {s}) ...\n", .{ base_addr[0..base_addr_len], peer_id[0..peer_id_len] });
    const dial_res = c.libp2p_host_dial(host, &base_addr[0], &peer_id[0], on_connected, state_ptr);
    if (dial_res != c.LIBP2P_SUCCESS) {
        const err_str = c.libp2p_error_string(dial_res);
        if (cstrOrNull(err_str)) |s| std.debug.print("Dial initiation failed: {s}\n", .{s});
        c.libp2p_host_destroy(host);
        c.libp2p_context_destroy(g_context.?);
        std.c.free(state_ptr);
        return;
    }

    // Run event loop until finished
    const run_res = c.libp2p_context_run(g_context.?);
    if (run_res != c.LIBP2P_SUCCESS) {
        const err_str = c.libp2p_error_string(run_res);
        if (cstrOrNull(err_str)) |s| std.debug.print("Context error: {s}\n", .{s});
    }

    c.libp2p_host_destroy(host);
    c.libp2p_context_destroy(g_context.?);
    g_context = null;

    std.debug.print("Client finished\n", .{});
}
