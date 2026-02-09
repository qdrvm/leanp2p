#include <cstdlib>
#include <iostream>
#include <string>
#include <hiredis/hiredis.h>
#include <sys/time.h>
#include <thread>
#include <optional>
#include <charconv>
#include <chrono>
#include <libp2p/log/simple.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <random>
#include <libp2p/common/sample_peer.hpp>
#include <libp2p/crypto/random_generator.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <libp2p/protocol/ping.hpp>
#include <libp2p/coro/spawn.hpp>
#include <fmt/format.h>
#include <libp2p/connection/stream.hpp>
#include <libp2p/common/weak_macro.hpp>

std::optional<std::string> getenv_opt(const char* name){
    if(const char* v = std::getenv(name)){
        return std::string(v);
    }
    else{
        return std::nullopt;
    }
}

redisContext* connect_redis(const std::string& host, int port, int timeout_ms, libp2p::log::Logger log){
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    redisContext* ctx = redisConnectWithTimeout(host.c_str(), port, timeout);

    if(!ctx || ctx->err){
        if(ctx){
            log->error("Failed to connect to redis:{}\n", ctx->errstr);
            redisFree(ctx);
        }
        return nullptr;
    }
    else{
        return ctx;
    }
}

bool wait_for_redis(redisContext* ctx, int timeout_ms){
    auto start_time = std::chrono::steady_clock::now();

    while(true){
        redisReply* reply = (redisReply*)redisCommand(ctx, "PING");
        if(reply){
            bool ok = reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "PONG";
            freeReplyObject(reply);
            if (ok) return true;
        }

        if (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(timeout_ms)){
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int parse_redis_port(std::string& redisAddr, libp2p::log::Logger log){
    size_t colon_pos = redisAddr.find(":");
    if (colon_pos == std::string::npos) {
        log->error("Could not find port in redis address, using default\n");
        return 6379;
    }
    std::string redisPortStr = redisAddr.substr(colon_pos + 1);
    int port{};
    auto [ptr, ec] = std::from_chars(redisPortStr.data(), redisPortStr.data() + redisPortStr.size(), port);
    if(ec == std::errc{}){
        return port;
    }
    else{
        log->error("Could not parse redis port, using default\n");
        return 6379;
    }
}

std::string parse_redis_host(const std::string &redisAddr, libp2p::log::Logger log) {
    size_t colonPos = redisAddr.find(':');
    if (colonPos == std::string::npos) {
        log->warn("No port separator found in redis address '{}', treating entire string as host", redisAddr);
        return redisAddr;
    }
    std::string host = redisAddr.substr(0, colonPos);
    if (host.empty()) {
        log->error("Empty host in redis address '{}'", redisAddr);
        return "localhost";
    }
    return host;
}

int main(){
    libp2p::simpleLoggingSystem();
    auto log = libp2p::log::createLogger("Ping");

    auto transport = getenv_opt("TRANSPORT");
    auto muxer = getenv_opt("MUXER"); //There for future use as skipped when transport=quic-v1
    auto secureChannel = getenv_opt("SECURE_CHANNEL"); //There for future use as skipped when transport=quic-v1
    auto isDialerStr = getenv_opt("IS_DIALER");
    std::string ip = getenv_opt("LISTENER_IP").value_or("0.0.0.0");
    std::string redisAddr = getenv_opt("REDIS_ADDR").value_or("redis:6379");
    auto testKey = getenv_opt("TEST_KEY");
    std::string debugStr = getenv_opt("DEBUG").value_or("false");

    int testTimeoutSeconds = 300;

    bool isDialer = *isDialerStr == "true";
    bool debug = debugStr == "true";

    if(!debug){
        log->setLevel(libp2p::log::Level::ERROR);
    }

    int redisPort = parse_redis_port(redisAddr, log);
    std::string redisHost = parse_redis_host(redisAddr, log);
    std::string redisKey = fmt::format("{}_listener_multiaddr", *testKey);
    log->info("Redis key: {}", redisKey);

    redisContext* ctx = connect_redis(redisHost, redisPort, testTimeoutSeconds * 1000, log); //Redis connection needs timeout in ms

    if(!wait_for_redis(ctx, testTimeoutSeconds)){
        redisFree(ctx);
        return 1;
    }

    unsigned int random_seed = static_cast<unsigned int>(std::random_device{}());
    auto sample_peer = libp2p::SamplePeer(random_seed, ip, libp2p::SamplePeer::samplePort(random_seed), libp2p::SamplePeer::Ed25519);

    std::shared_ptr<boost::asio::io_context> io_context;
    std::shared_ptr<libp2p::host::BasicHost> host;
    std::shared_ptr<libp2p::crypto::random::CSPRNG> random;
    if(*transport == "quic-v1"){
        auto injector = libp2p::injector::makeHostInjector(
            libp2p::injector::useKeyPair(sample_peer.keypair),
            libp2p::injector::useTransportAdaptors<libp2p::transport::QuicTransport>()
        );

        io_context = injector.create<std::shared_ptr<boost::asio::io_context>>();
        host = injector.create<std::shared_ptr<libp2p::host::BasicHost>>();
        random = injector.create<std::shared_ptr<libp2p::crypto::random::CSPRNG>>();

        host->listenProtocol(injector.create<std::shared_ptr<libp2p::protocol::Ping>>());
    }
    else{
        log->error("Unsupported transport protocol\n");
        return 1;
    }

    libp2p::protocol::PingConfig pingConfig{};
    auto ping = std::make_shared<libp2p::protocol::Ping>(io_context, host, random, pingConfig);

    if (not host->listen(sample_peer.listen)) {
        std::println("Error listening on {}", sample_peer.listen);
        return 1;
    }

    host->start();
    ping->start();
    log->info("Connection string: {}", sample_peer.connect);

    if(isDialer){
        redisReply* replyListenAddr = (redisReply*)redisCommand(ctx, "BLPOP %s %d", redisKey.c_str(), testTimeoutSeconds);
        if(replyListenAddr){
            if(replyListenAddr->type == REDIS_REPLY_ERROR){
                log->error("Redis BLPOP error: {}", replyListenAddr->str);
                freeReplyObject(replyListenAddr);
                redisFree(ctx);
                return 1;
            }
            bool ok = replyListenAddr->type == REDIS_REPLY_ARRAY && replyListenAddr->elements == 2;
            if(ok){
                std::string listenAddr = replyListenAddr->element[1]->str;
                log->info("Retrieved listener address from redis: {}", listenAddr);
                auto address_res = libp2p::Multiaddress::create(listenAddr);
                auto address = address_res.value();
                auto peer_id_res = address.getPeerId();
                auto peer_id = libp2p::PeerId::fromBase58(peer_id_res.value());
                libp2p::peer::PeerInfo connect_info = {peer_id.value(), {address}};
                freeReplyObject(replyListenAddr);

                libp2p::coroSpawn(*io_context, [&]() -> libp2p::Coro<void> {
                    log->info("Connecting to {}", connect_info.addresses.at(0));
                    auto handShakeStart = std::chrono::steady_clock::now();
                    auto connect_res = (co_await host->connect(connect_info));
                    if (not connect_res.has_value()) {
                        log->error("Failed to connect to peer");
                        io_context->stop();
                        co_return;
                    }
                    else{
                        log->info("Connected successfully");
                        auto connection = connect_res.value();

                        auto ping_res = (co_await ping->ping(connection, std::chrono::seconds(testTimeoutSeconds)));
                        if(not ping_res.has_value()){
                            log->error("Ping failed");
                            io_context->stop();
                            co_return;
                        }
                        else{
                            auto handShakeEnd = std::chrono::steady_clock::now();
                            log->info("Ping successful");
                            auto ping_rtt = ping_res.value();
                            auto handShakePlusOneRTT = std::chrono::duration_cast<std::chrono::milliseconds>(handShakeEnd - handShakeStart);

                            // Printing out results in stdout
                            std::cout << "latency:\n";
                            std::cout << fmt::format("  handshake_plus_one_rtt: {}\n", handShakePlusOneRTT.count());
                            std::cout << fmt::format("  ping_rtt: {}\n", ping_rtt.count());
                            std::cout << "  unit: ms\n";

                            io_context->stop();
                        }
                    }
                });

                io_context->run();
                redisFree(ctx);
            }
            else{
                log->error("Failed to wait for listener to be ready - unexpected reply type: {} (elements: {})", 
                          replyListenAddr->type, 
                          replyListenAddr->type == REDIS_REPLY_ARRAY ? replyListenAddr->elements : 0);
                freeReplyObject(replyListenAddr);
                redisFree(ctx);
                return 1;
            }
        }
        else{
            log->error("Failed to get listener address from redis - {}", ctx->errstr);
            redisFree(ctx);
            return 1;
        }
    }else{
        std::string connectStr = std::string(sample_peer.connect.getStringAddress());
        log->info("Pushing connect string {}", connectStr);
        redisReply* replyListenAddr = (redisReply*)redisCommand(ctx, "RPUSH %s %s", redisKey.c_str(), connectStr.c_str());
        if(replyListenAddr){
            if(replyListenAddr->type == REDIS_REPLY_ERROR){
                log->error("Redis RPUSH error: {}", replyListenAddr->str);
                freeReplyObject(replyListenAddr);
                redisFree(ctx);
                return 1;
            }
            bool ok = replyListenAddr->type == REDIS_REPLY_INTEGER;
            if(ok){
                log->info("Listener address pushed to redis");
                freeReplyObject(replyListenAddr);
                
                boost::asio::steady_timer timeout_timer(*io_context);
                timeout_timer.expires_after(std::chrono::seconds(testTimeoutSeconds));
                timeout_timer.async_wait([&](const boost::system::error_code& ec) {
                    if(!ec){
                        log->info("Test timeout reached");
                        io_context->stop();
                    }
                });

                io_context->run();

                log->info("Listener exiting");
                redisFree(ctx);
                return 1;
            }
            else{
                log->error("Failed to push address to redis - unexpected reply type: {}", replyListenAddr->type);
                freeReplyObject(replyListenAddr);
                redisFree(ctx);
                return 1;
            }
        }
        else{
            log->error("Failed to get status of address push from redis - {}", ctx->errstr);
            redisFree(ctx);
            return 1;
        }
    }

    return 1;
}