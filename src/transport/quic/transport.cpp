/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/coro/asio.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <libp2p/security/tls/ssl_context.hpp>
#include <libp2p/transport/quic/connection.hpp>
#include <libp2p/transport/quic/engine.hpp>
#include <libp2p/transport/quic/listener.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <libp2p/transport/tcp/tcp_util.hpp>

namespace libp2p::transport {
  QuicTransport::QuicTransport(
      std::shared_ptr<boost::asio::io_context> io_context,
      const security::SslContext &ssl_context,
      const muxer::MuxedConnectionConfig &mux_config,
      const peer::IdentityManager &id_mgr,
      std::shared_ptr<crypto::marshaller::KeyMarshaller> key_codec)
      : io_context_{std::move(io_context)},
        ssl_context_{ssl_context.quic},
        mux_config_{mux_config},
        local_peer_{id_mgr.getId()},
        key_codec_{std::move(key_codec)},
        resolver_{*io_context_},
        client4_{makeClient(boost::asio::ip::udp::v4())} {}

  CoroOutcome<std::shared_ptr<connection::CapableConnection>>
  QuicTransport::dial(const PeerId &peer, Multiaddress address) {
    BOOST_OUTCOME_CO_TRY(auto info, detail::asQuic(address));
    std::string host;
    if (auto ip = std::get_if<boost::asio::ip::address>(&info.ip)) {
      host = ip->to_string();
    } else if (auto dns = std::get_if<detail::Dns>(&info.ip)) {
      host = dns->name;
    } else {
      co_return make_error_code(boost::system::errc::protocol_not_supported);
    }
    auto port = std::to_string(info.port);
    BOOST_OUTCOME_CO_TRY(auto results,
                         coroOutcome(co_await resolver_.async_resolve(
                             host, port, useCoroOutcome)));
    if (results.empty()) {
      co_return make_error_code(boost::system::errc::host_unreachable);
    }
    auto remote = results.begin()->endpoint();
    auto v4 = remote.protocol() == boost::asio::ip::udp::v4();
    auto &client = v4 ? client4_ : client6_;
    auto conn_result = co_await client->connect(remote, peer);
    if (!conn_result) {
      co_return conn_result.as_failure();
    }
    co_return std::static_pointer_cast<connection::CapableConnection>(
        conn_result.value());
  }

  std::shared_ptr<TransportListener> QuicTransport::createListener() {
    return std::make_shared<QuicListener>(
        io_context_, ssl_context_, mux_config_, local_peer_, key_codec_);
  }

  bool QuicTransport::canDial(const Multiaddress &ma) const {
    return detail::asQuic(ma).has_value();
  }

  std::shared_ptr<lsquic::Engine> QuicTransport::makeClient(
      boost::asio::ip::udp protocol) const {
    return std::make_shared<lsquic::Engine>(io_context_,
                                            ssl_context_,
                                            mux_config_,
                                            local_peer_,
                                            key_codec_,
                                            boost::asio::ip::udp::socket{
                                                *io_context_,
                                                protocol,
                                            },
                                            true);
  }
}  // namespace libp2p::transport
