/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/connection/stream.hpp>
#include <libp2p/peer/stream_protocols.hpp>

namespace libp2p::protocol {

  /**
   * @brief Base class for user-defined protocols.
   *
   * @example
   * {@code}
   * struct EchoProtocol: public BaseProtocol {
   *   ...
   * };
   *
   * std::shared_ptr<Network> nw = std::make_shared<NetworkImpl>(...);
   * std::shared_ptr<BaseProtocol> p = std::make_shared<EchoProtocol>();
   *
   * // register protocol handler (server side callback will be executed
   * // when client opens a stream to us)
   * nw->addProtocol(p);
   * {@nocode}
   */
  class BaseProtocol {
   public:
    virtual ~BaseProtocol() = default;

    virtual StreamProtocols getProtocolIds() const = 0;

    /**
     * @brief Handler that is executed on responder (server) side of the
     * protocol.
     * @param stream_res, which was received
     */
    virtual void handle(std::shared_ptr<Stream> stream) = 0;
  };

}  // namespace libp2p::protocol
