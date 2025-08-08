/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

#include <libp2p/basic/write.hpp>
#include <libp2p/protocol/echo/echo.hpp>
#include <qtils/bytestr.hpp>
#include <utility>

namespace libp2p::protocol {

  void Echo::handle(std::shared_ptr<connection::Stream> stream) {
    boost::asio::co_spawn(
        *io_context_,
        [this, stream]() mutable -> Coro<void> {
          while (stream && !stream->isClosed()) {
            co_await doRead(stream);
            // If max_server_repeats is reached and not infinite, break
            if (!repeat_infinitely_ && config_.max_server_repeats == 0) {
              break;
            }
          }
        },
        boost::asio::detached);
  }

  Coro<void> Echo::doRead(std::shared_ptr<connection::Stream> stream) {
    if (!repeat_infinitely_ && config_.max_server_repeats == 0) {
      stop(std::move(stream));
      co_return;
    }
    std::vector<uint8_t> buf;
    size_t max_recv_size = 65536;
    if (config_.max_recv_size < max_recv_size) {
      max_recv_size = config_.max_recv_size;
    }
    buf.resize(max_recv_size);
    outcome::result<size_t> rread = co_await stream->readSome(buf, buf.size());

    // onRead
    if (!rread) {
      if (!stream->isClosed()) {
        log_->error("error happened during read: {}", rread.error());
      }
      stop(std::move(stream));
      co_return;
    }
    static constexpr size_t kMsgSizeThreshold = 120;
    if (rread.value() < kMsgSizeThreshold) {
      log_->debug(
          "read message: {}",
          std::string{buf.begin(),
                      // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
                      buf.begin() + rread.value()});
    } else {
      log_->debug("read {} bytes", rread.value());
    }
    size_t size = rread.value();

    // doWrite
    if (size == 0) {
      log_->debug("read zero bytes, closing stream");
      stop(std::move(stream));
      co_return;
    }
    auto write_buf = std::vector<uint8_t>(
        buf.begin(),
        // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
        buf.begin() + size);
    auto rwrite = co_await write(stream, write_buf);

    // onWrite
    if (rwrite.has_error()) {
      log_->error("error happened during write: {}", rwrite.error().message());
      stop(std::move(stream));
      co_return;
    }
    if (buf.size() < 120) {
      log_->info("written message: {}", qtils::byte2str(buf));
    } else {
      log_->info("written {} bytes", buf.size());
    }

    if (!repeat_infinitely_) {
      --config_.max_server_repeats;
    }
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
