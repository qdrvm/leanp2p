/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/basic/readwriter.hpp>
#include <libp2p/connection/capable_connection.hpp>
#include <libp2p/multi/multiaddress.hpp>

namespace libp2p::peer {
  class PeerId;
}

namespace libp2p::connection {

  /**
   * Stream over some connection, allowing to write/read to/from that connection
   * @note the user MUST WAIT for the completion of method from this list
   * before calling another method from this list:
   *    - write
   *    - writeSome
   *    - close
   *    - adjustWindowSize
   *    - reset
   * Also, 'read' & 'readSome' are in another tuple. This behaviour results in
   * possibility to read and write simultaneously, but double read or write is
   * forbidden
   */
  struct Stream : public basic::ReadWriter {
    enum class Error {
      STREAM_INTERNAL_ERROR = 1,
      STREAM_INVALID_ARGUMENT,
      STREAM_PROTOCOL_ERROR,
      STREAM_IS_READING,
      STREAM_NOT_READABLE,
      STREAM_NOT_WRITABLE,
      STREAM_CLOSED_BY_HOST,
      STREAM_CLOSED_BY_PEER,
      STREAM_RESET_BY_HOST,
      STREAM_RESET_BY_PEER,
      STREAM_INVALID_WINDOW_SIZE,
      STREAM_WRITE_OVERFLOW,
      STREAM_RECEIVE_OVERFLOW,
    };

    using VoidResultHandlerFunc = std::function<void(outcome::result<void>)>;

    ~Stream() override = default;

    /**
     * Close a stream, indicating we are not going to write to it anymore; the
     * other side, however, can write to it, if it was not closed from there
     * before
     * @param cb to be called, when the stream is closed, or error happens
     */
    virtual outcome::result<void> close() = 0;

    /**
     * @brief Close this stream entirely; this normally means an error happened,
     * so it should not be used just to close the stream
     */
    virtual void reset() = 0;

    /**
     * Is that stream opened over a connection, which was an initiator?
     */
    virtual outcome::result<bool> isInitiator() const = 0;

    virtual bool isClosed() const = 0;

    /**
     * Get a peer, which the stream is connected to
     * @return id of the peer
     */
    virtual outcome::result<peer::PeerId> remotePeerId() const = 0;

    /**
     * Get a local multiaddress
     * @return address or error
     */
    virtual outcome::result<multi::Multiaddress> localMultiaddr() const = 0;

    /**
     * Get a multiaddress, to which the stream is connected
     * @return multiaddress or error
     */
    virtual outcome::result<multi::Multiaddress> remoteMultiaddr() const = 0;
  };
}  // namespace libp2p::connection

OUTCOME_HPP_DECLARE_ERROR(libp2p::connection, Stream::Error)
