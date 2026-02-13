#include <libp2p/common/sample_peer.hpp>
#include <libp2p/connection/stream.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/log/simple.hpp>
#include <libp2p/protocol/ping.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <random>
#include "../common.hpp"

int main() {
  setlinebuf(stdout);
  setlinebuf(stderr);

  libp2p::simpleLoggingSystem();
  auto log = libp2p::log::createLogger("Ping");

  auto transport = getenv_opt("TRANSPORT");
  auto muxer = getenv_opt(
      "MUXER");  // There for future use as skipped when transport=quic-v1
  auto secureChannel =
      getenv_opt("SECURE_CHANNEL");  // There for future use as skipped when
                                     // transport=quic-v1
  auto isDialer = getenv_opt("IS_DIALER").and_then(parseBool).value_or(false);
  std::string listener_ip =
      getenv_opt("LISTENER_IP")
          .value_or("0.0.0.0");  // When we create sample peer, it binds on
                                 // 0.0.0.0 by default
  std::string redisAddr = getenv_opt("REDIS_ADDR").value_or("redis");
  auto testKey = getenv_opt("TEST_KEY");
  bool debug = getenv_opt("DEBUG").and_then(parseBool).value_or(false);

  TestTimeout timeout{std::chrono::seconds{300}};

  if (!debug) {
    log->setLevel(libp2p::log::Level::ERROR);
  }

  // Detect actual network IP if listener_ip is 0.0.0.0
  // This has to be done manually right now
  std::string ip = listener_ip;
  if (not isDialer and listener_ip == "0.0.0.0") {
    auto detected_ip = get_first_network_ip(log);
    if (detected_ip.has_value()) {
      ip = detected_ip.value();
      SL_INFO(log, "Using detected network IP: {}", ip);
    } else {
      SL_WARN(log, "Could not detect network IP, using 0.0.0.0");
    }
  }

  std::string redisKey = fmt::format("{}_listener_multiaddr", *testKey);
  SL_INFO(log, "Redis key: {}", redisKey);

  auto redis = testRedisConnect(log, redisAddr, timeout);

  unsigned int random_seed = static_cast<unsigned int>(std::random_device{}());
  auto sample_peer =
      libp2p::SamplePeer(random_seed,
                         ip,
                         libp2p::SamplePeer::samplePort(random_seed),
                         libp2p::SamplePeer::Ed25519);

  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<libp2p::host::BasicHost> host;
  std::shared_ptr<libp2p::protocol::Ping> ping;
  std::shared_ptr<void> injector_lifetime;
  if (*transport == "quic-v1") {
    auto make_injector = [&] {
      return libp2p::injector::makeHostInjector(
          libp2p::injector::useKeyPair(sample_peer.keypair),
          libp2p::injector::useTransportAdaptors<
              libp2p::transport::QuicTransport>());
    };
    auto injector =
        std::make_shared<decltype(make_injector())>(make_injector());
    injector_lifetime = injector;

    io_context = injector->create<std::shared_ptr<boost::asio::io_context>>();
    host = injector->create<std::shared_ptr<libp2p::host::BasicHost>>();
    ping = injector->create<std::shared_ptr<libp2p::protocol::Ping>>();

  } else {
    SL_FATAL(log, "Unsupported transport protocol");
  }

  host->start();
  host->listenProtocol(ping);
  ping->start();

  if (isDialer) {
    auto address_str =
        TRY_OR_SL_FATAL(redis->blpop(redisKey, timeout.remaining()),
                        log,
                        "Retrieve listener address error: {}");
    SL_INFO(log, "Retrieved listener address from redis: {}", address_str);
    auto address = TRY_OR_SL_FATAL(libp2p::Multiaddress::create(address_str),
                                   log,
                                   "Parse listener address error: {}");
    auto peer_id_str = address.getPeerId();
    if (not peer_id_str.has_value()) {
      SL_FATAL(log, "No peer id in listener address");
    }
    auto peer_id = TRY_OR_SL_FATAL(libp2p::PeerId::fromBase58(*peer_id_str),
                                   log,
                                   "Parse listener address peer id");
    libp2p::peer::PeerInfo connect_info{peer_id, {address}};

    int exitStatus = 0;
    libp2p::coroSpawn(*io_context, [&]() -> libp2p::Coro<void> {
      SL_INFO(log, "Connecting to {}", connect_info.addresses.at(0));
      auto handShakeStart = std::chrono::steady_clock::now();
      auto connection = TRY_OR_SL_FATAL(
          co_await host->connect(connect_info), log, "Connect error: {}");
      SL_INFO(log, "Connected successfully");
      auto ping_rtt = TRY_OR_SL_FATAL(
          co_await ping->ping(connection, timeout.remainingMs()),
          log,
          "Ping error: {}");
      auto handShakeEnd = std::chrono::steady_clock::now();
      SL_INFO(log, "Ping successful");

      auto handShakePlusOneRTT_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              handShakeEnd - handShakeStart);
      auto ping_rtt_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(ping_rtt);

      SL_INFO(log, "latency:");
      SL_INFO(
          log, "  handshake_plus_one_rtt: {}", handShakePlusOneRTT_ms.count());
      SL_INFO(log, "  ping_rtt: {}", ping_rtt_ms.count());
      SL_INFO(log, "  unit: ms");

      io_context->stop();
    });
    io_context->run_for(timeout.remaining());
  } else {
    TRY_OR_SL_FATAL(host->listen(sample_peer.listen),
                    log,
                    "Error listening on {}: {}",
                    sample_peer.listen);
    std::string address{sample_peer.connect.getStringAddress()};
    SL_INFO(log, "Pushing connect address {}", address);
    TRY_OR_SL_FATAL(
        redis->rpush(redisKey, address), log, "Push connect address error: {}");
    SL_INFO(log, "Listener address pushed to redis");
    io_context->run_for(timeout.remaining());
    SL_INFO(log, "Listener exiting");
  }
  return EXIT_SUCCESS;
}
