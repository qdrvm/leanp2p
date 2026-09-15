/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <functional>
#include <qtils/byte_vec.hpp>
#include <qtils/enum_error_code.hpp>

namespace libp2p::connection {
  struct CapableConnection;
}  // namespace libp2p::connection

namespace libp2p {
  using OnDatagram = std::function<void(
      std::shared_ptr<connection::CapableConnection>, qtils::ByteVec)>;

  enum class DatagramError {
    /// Transport does not implement unreliable datagrams at all.
    UNSUPPORTED,
    /// Datagrams were not negotiated: disabled locally, or the peer did not
    /// advertise `max_datagram_frame_size`.
    NOT_NEGOTIATED,
    /// Payload exceeds the maximum negotiated with the peer.
    TOO_LARGE,
    /// Connection closed before the datagram could be sent.
    CONN_CLOSED,
    /// Too many datagrams are already waiting to be sent.
    QUEUE_FULL,
    /// `PollFuture::cancel` was called before the datagram was sent.
    CANCELLED,
  };
  Q_ENUM_ERROR_CODE(DatagramError) {
    using E = decltype(e);
    switch (e) {
      case E::UNSUPPORTED:
        return "UNSUPPORTED";
      case E::NOT_NEGOTIATED:
        return "NOT_NEGOTIATED";
      case E::TOO_LARGE:
        return "TOO_LARGE";
      case E::CONN_CLOSED:
        return "CONN_CLOSED";
      case E::QUEUE_FULL:
        return "QUEUE_FULL";
      case E::CANCELLED:
        return "CANCELLED";
    }
    abort();
  }

  struct OnDatagramConfig {
    OnDatagramConfig() = default;

    /**
     * Advertise `max_datagram_frame_size` to peers. Datagrams can only be
     * sent or received on a connection when both peers enable them.
     */
    bool enable_datagram = false;
    /**
     * Invoked for every received datagram. When empty, incoming datagrams
     * are discarded.
     */
    OnDatagram on_datagram;
    /**
     * Upper bound on datagrams queued for sending per connection. Further
     * sends fail with `DatagramError::QUEUE_FULL` instead of growing the
     * queue without limit.
     */
    size_t max_send_queue = 128;
  };
  using OnDatagramConfigPtr = std::shared_ptr<OnDatagramConfig>;
}  // namespace libp2p
