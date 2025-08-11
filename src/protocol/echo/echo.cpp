/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <boost/asio/io_context.hpp>

#include <libp2p/basic/write.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/protocol/echo/echo.hpp>
#include <libp2p/transport/quic/error.hpp>
#include <qtils/bytestr.hpp>
#include <utility>

namespace libp2p::protocol {

  void Echo::handle(std::shared_ptr<connection::Stream> stream) {
    coroSpawn(*io_context_, doRead(stream));
  }

  Coro<void> Echo::doRead(std::shared_ptr<connection::Stream> stream) {
    auto max_repeat = config_.max_server_repeats;
    std::vector<uint8_t> buf;
    while (repeat_infinitely_ or max_repeat != 0) {
      buf.resize(config_.max_recv_size);
      auto read_result = co_await stream->readSome(buf);
      if (not read_result.has_value()) {
        if (read_result.error() != QuicError::STREAM_CLOSED) {
          log_->error("error happened during read: {}", read_result.error());
        }
        break;
      }
      buf.resize(read_result.value());
      static constexpr size_t kMsgSizeThreshold = 120;
      if (buf.size() < kMsgSizeThreshold) {
        log_->info("read message: {}", qtils::byte2str(buf));
      } else {
        log_->info("read {} bytes", buf.size());
      }
      auto write_result = co_await write(stream, buf);
      if (not write_result.has_value()) {
        log_->error("error happened during write: {}", write_result.error());
        break;
      }
      if (buf.size() < kMsgSizeThreshold) {
        log_->info("written message: {}", qtils::byte2str(buf));
      } else {
        log_->info("written {} bytes", buf.size());
      }
      if (not repeat_infinitely_) {
        --max_repeat;
      }
    }
    stop(std::move(stream));
  }

  void Echo::stop(std::shared_ptr<connection::Stream> stream) {
    if (auto res = stream->close(); not res) {
      log_->error("cannot close the stream: {}", res.error());
    }
  }

  peer::ProtocolName Echo::getProtocolId() const {
    return "/echo/1.0.0";
  }

  Echo::Echo(std::shared_ptr<boost::asio::io_context> io_context,
             EchoConfig config)
      : io_context_(std::move(io_context)),
        config_(config),
        repeat_infinitely_{config.max_server_repeats == 0} {}

  // std::shared_ptr<ClientEchoSession> Echo::createClient(
  //     const std::shared_ptr<connection::Stream> &stream) {
  //   return std::make_shared<ClientEchoSession>(stream);
  // }

}  // namespace libp2p::protocol
