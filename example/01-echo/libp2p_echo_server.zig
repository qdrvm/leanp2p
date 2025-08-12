const std = @import("std");

const c = @cImport({
    @cInclude("signal.h");
    @cInclude("libp2p/c/libp2p_c.h");
});

// Global context for signal handling
var g_context: ?*c.libp2p_context_t = null;

// State stored per-connection
const EchoState = extern struct {
    read_buffer: [1024]u8 = undefined,
    bytes_to_echo: usize = 0,
};

fn cstrOrNull(p: [*c]const u8) ?[]const u8 {
    if (p == null) return null;
    const z: [*:0]const u8 = @ptrCast(p);
    return std.mem.span(z);
}

// Signal handler: stop the context to gracefully shutdown
export fn sig_handler(sig: c_int) callconv(.C) void {
    _ = sig; // unused
    if (g_context) |ctx| {
        c.libp2p_context_stop(ctx);
    }
}

// Forward declarations (not required in Zig; only keep definitions below)
export fn on_read_complete(stream: ?*c.libp2p_stream_t, data: [*c]const u8, size: usize, err_code: c.libp2p_error_t, user_data: ?*anyopaque) callconv(.C) void {
    var state_ptr: *EchoState = @ptrCast(@alignCast(user_data.?));

    if (err_code != c.LIBP2P_SUCCESS) {
        if (err_code != c.LIBP2P_ERROR_IO_ERROR) { // ignore normal closure
            const err_str = c.libp2p_error_string(err_code);
            if (cstrOrNull(err_str)) |s| std.debug.print("Error reading from stream: {s}\n", .{s});
        }
        std.c.free(state_ptr);
        return;
    }

    if (size == 0) {
        std.debug.print("Stream closed by peer\n", .{});
        std.c.free(state_ptr);
        return;
    }

    if (size < 120) {
        const slice = data[0..size];
        std.debug.print("Received message ({d} bytes): {s}\n", .{ size, slice });
    } else {
        std.debug.print("Received {d} bytes\n", .{size});
    }

    state_ptr.bytes_to_echo = size;
    const write_res = c.libp2p_stream_write(stream, data, size, on_write_complete, state_ptr);
    if (write_res != c.LIBP2P_SUCCESS) {
        if (cstrOrNull(c.libp2p_error_string(write_res))) |s| std.debug.print("Failed to initiate write: {s}\n", .{s});
        std.c.free(state_ptr);
    }
}

export fn on_write_complete(stream: ?*c.libp2p_stream_t, bytes_written: usize, err_code: c.libp2p_error_t, user_data: ?*anyopaque) callconv(.C) void {
    var state_ptr: *EchoState = @ptrCast(@alignCast(user_data.?));

    if (err_code != c.LIBP2P_SUCCESS) {
        if (cstrOrNull(c.libp2p_error_string(err_code))) |s| std.debug.print("Error writing to stream: {s}\n", .{s});
        std.c.free(state_ptr);
        return;
    }

    if (state_ptr.bytes_to_echo < 120) {
        std.debug.print("Echoed message ({d} bytes)\n", .{bytes_written});
    } else {
        std.debug.print("Echoed {d} bytes\n", .{bytes_written});
    }

    const read_res = c.libp2p_stream_read(stream, &state_ptr.read_buffer[0], state_ptr.read_buffer.len, on_read_complete, state_ptr);
    if (read_res != c.LIBP2P_SUCCESS) {
        if (cstrOrNull(c.libp2p_error_string(read_res))) |s| std.debug.print("Failed to initiate read: {s}\n", .{s});
        std.c.free(state_ptr);
    }
}

// Called when a new stream is opened with the echo protocol
export fn echo_protocol_handler(stream: ?*c.libp2p_stream_t, user_data: ?*anyopaque) callconv(.C) void {
    _ = user_data;
    std.debug.print("New echo protocol stream opened\n", .{});

    const mem = std.c.malloc(@sizeOf(EchoState));
    if (mem == null) {
        std.debug.print("Failed to allocate memory for echo state\n", .{});
        _ = c.libp2p_stream_close(stream);
        return;
    }
    var state_ptr: *EchoState = @ptrCast(@alignCast(mem.?));
    state_ptr.* = EchoState{}; // init fields

    const res = c.libp2p_stream_read(stream, &state_ptr.read_buffer[0], state_ptr.read_buffer.len, on_read_complete, state_ptr);
    if (res != c.LIBP2P_SUCCESS) {
        if (cstrOrNull(c.libp2p_error_string(res))) |s| std.debug.print("Failed to start reading from stream: {s}\n", .{s});
        std.c.free(state_ptr);
        _ = c.libp2p_stream_close(stream);
        return;
    }
}

pub fn main() !void {
    const gpa = std.heap.c_allocator; // use C allocator for C interop strings

    // Setup signal handlers
    _ = c.signal(c.SIGINT, sig_handler);
    _ = c.signal(c.SIGTERM, sig_handler);

    // Set logging level
    c.libp2p_set_log_level(c.LIBP2P_LOG_INFO);

    std.debug.print("LibP2P Echo Server (Zig via C API)\n", .{});
    std.debug.print("===============================\n", .{});

    // Create context
    g_context = c.libp2p_context_create();
    if (g_context == null) {
        std.debug.print("Failed to create libp2p context\n", .{});
        return;
    }

    // Create host with deterministic seed
    const keypair_seed: [*:0]const u8 = "echo_server_seed_0";
    const host = c.libp2p_host_create(g_context.?, keypair_seed);
    if (host == null) {
        std.debug.print("Failed to create libp2p host\n", .{});
        c.libp2p_context_destroy(g_context.?);
        g_context = null;
        return;
    }

    // Register protocol handler
    const reg = c.libp2p_host_register_protocol(host, "/echo/1.0.0", echo_protocol_handler, null);
    if (reg != c.LIBP2P_SUCCESS) {
        if (cstrOrNull(c.libp2p_error_string(reg))) |s| std.debug.print("Failed to register echo protocol: {s}\n", .{s});
        c.libp2p_host_destroy(host);
        c.libp2p_context_destroy(g_context.?);
        g_context = null;
        return;
    }

    // Determine listen addr
    const args = try std.process.argsAlloc(std.heap.page_allocator);
    defer std.process.argsFree(std.heap.page_allocator, args);

    var listen_addr_z: [*:0]const u8 = "/ip4/127.0.0.1/udp/0/quic-v1";
    var listen_addr_alloc: ?[:0]u8 = null;
    defer if (listen_addr_alloc) |buf| gpa.free(buf);

    if (args.len >= 2) {
    listen_addr_alloc = try std.fmt.allocPrintZ(gpa, "{s}", .{args[1]});
    listen_addr_z = listen_addr_alloc.?.ptr;
    }

    // Listen and start
    const lres = c.libp2p_host_listen(host, listen_addr_z);
    if (lres != c.LIBP2P_SUCCESS) {
        if (cstrOrNull(c.libp2p_error_string(lres))) |s| std.debug.print("Failed to listen on {s}: {s}\n", .{ listen_addr_z, s });
        c.libp2p_host_destroy(host);
        c.libp2p_context_destroy(g_context.?);
        g_context = null;
        return;
    }

    const sres = c.libp2p_host_start(host);
    if (sres != c.LIBP2P_SUCCESS) {
        if (cstrOrNull(c.libp2p_error_string(sres))) |s| std.debug.print("Failed to start host: {s}\n", .{s});
        c.libp2p_host_destroy(host);
        c.libp2p_context_destroy(g_context.?);
        g_context = null;
        return;
    }

    std.debug.print("Echo server started successfully\n", .{});

    var resolved_listen: [*:0]const u8 = listen_addr_z;
    if (c.libp2p_host_default_listen(host)) |def_listen| {
        const z: [*:0]const u8 = @ptrCast(def_listen);
        resolved_listen = z;
    }

    if (c.libp2p_host_peer_id(host)) |peer_id_c| {
        if (cstrOrNull(peer_id_c)) |peer_id| {
            std.debug.print("Peer ID: {s}\n", .{peer_id});
        }
        std.debug.print("Listening on: {s}\n", .{resolved_listen});
        if (cstrOrNull(peer_id_c)) |peer_id| {
            std.debug.print("Full multiaddr: {s}/p2p/{s}\n", .{ resolved_listen, peer_id });
        }
    } else {
        std.debug.print("Listening on: {s}\n", .{resolved_listen});
    }

    std.debug.print("Protocol: /echo/1.0.0\n", .{});
    std.debug.print("Press Ctrl+C to stop the server\n\n", .{});

    const run_res = c.libp2p_context_run(g_context.?);
    if (run_res != c.LIBP2P_SUCCESS) {
        if (cstrOrNull(c.libp2p_error_string(run_res))) |s| std.debug.print("Context run failed: {s}\n", .{s});
    }

    std.debug.print("Shutting down...\n", .{});
    c.libp2p_host_destroy(host);
    c.libp2p_context_destroy(g_context.?);
    g_context = null;

    std.debug.print("Echo server stopped\n", .{});
}
