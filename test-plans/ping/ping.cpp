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
    std::string redisPortStr = redisAddr.substr(redisAddr.find(":"), redisAddr.length());
    int port{};
    auto [ptr, ec] = std::from_chars(redisPortStr.data(), redisPortStr.data() + redisPortStr.size(), port);
    if(ec == std::errc{}){
        return port;
    }
    else{
        log->error("Could not parse reddis port, using default\n");
        return 6379;
    }
}

std::shared_ptr<libp2p::host::BasicHost> make_host(){}

int main(){
    libp2p::simpleLoggingSystem();
    auto log = libp2p::log::createLogger("Ping");

    auto transport = getenv_opt("transport");
    auto muxer = getenv_opt("muxer");
    auto secureChannel = getenv_opt("security");
    auto isDialerStr = getenv_opt("is_dialer");
    std::string ip = getenv_opt("ip").value_or("0.0.0.0");
    std::string redisAddr = getenv_opt("redis_addr").value_or("redis:6379");
    auto testTimeoutStr = getenv_opt("test_timeout_seconds");

    int testTimeout = 3 * 60;
    if (testTimeoutStr) {
        int value{};
        auto [ptr, ec] = std::from_chars(testTimeoutStr->data(), testTimeoutStr->data() + testTimeoutStr->size(), value);
        if(ec == std::errc{}){
            testTimeout = value;
        }
        else{
            log->error("Invalid test timeout, using default\n");
            return 1;
        }
    }

    int redisPort = parse_redis_port(redisAddr, log);

    redisContext* ctx = connect_redis(ip, redisPort, testTimeout, log);

    if(!wait_for_redis(ctx, testTimeout)){
        redisFree(ctx);
        return 1;
    }

    bool isDialer = *isDialerStr == "true";

    unsigned int random_seed = static_cast<unsigned int>(std::random_device{}());
    auto sample_peer = libp2p::SamplePeer::makeEd25519(random_seed);

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
        redisReply* replyListenAddr = (redisReply*)redisCommand(ctx, fmt::format("BLPOP listenAddr {}", testTimeout).c_str());
        if(replyListenAddr){
            bool ok = replyListenAddr->type == REDIS_REPLY_STATUS;
            if(ok){
                std::string listenAddr = replyListenAddr->str;
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

                        auto ping_res = (co_await ping->ping(connection, std::chrono::milliseconds(testTimeout)));
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
                            std::cout << " unit: ms\n";

                            io_context->stop();
                        }
                    }
                });
            }
            else{
                log->error("Failed to wait for listener to be ready");
                redisFree(ctx);
                return 1;
            }
        }
        else{
            log->error("Failed to get listener address from redis");
            redisFree(ctx);
            return 1;
        }
    }else{
        redisReply* replyListenAddr = (redisReply*)redisCommand(ctx, fmt::format("RPUSH listenAddr {}", testTimeout).c_str());
        if(replyListenAddr){
            bool ok = replyListenAddr->type == REDIS_REPLY_STATUS;
            freeReplyObject(replyListenAddr);
            if(ok){
                log->info("Listener address pushed to redis");
                
                boost::asio::steady_timer timeout_timer(*io_context);
                timeout_timer.expires_after(std::chrono::seconds(testTimeout));
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
                log->error("Failed to push address to redis");
                redisFree(ctx);
                return 1;
            }
        }
        else{
            log->error("Failed to get status of address push from redis");
            redisFree(ctx);
            return 1;
        }
    }

    return 0;
}