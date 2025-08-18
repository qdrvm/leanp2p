/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/coro/channel.hpp>
#include <libp2p/coro/coro.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/protocol/base_protocol.hpp>
#include <libp2p/protocol/gossip/config.hpp>
#include <libp2p/protocol/gossip/time_cache.hpp>
#include <qtils/bytes_std_hash.hpp>
#include <unordered_set>

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace libp2p::crypto {
  class CryptoProvider;
}  // namespace libp2p::crypto

namespace libp2p::host {
  class BasicHost;
}  // namespace libp2p::host

namespace libp2p::peer {
  class IdentityManager;
}  // namespace libp2p::peer

namespace libp2p::protocol::gossip {
  using StreamPtr = std::shared_ptr<connection::Stream>;

  class Gossip;

  struct PublishConfigSigning {
    Seqno last_seq_no;
  };

  struct Rpc {
    void subscribe(TopicHash topic_hash, bool subscribe);

    std::unordered_map<TopicHash, bool, qtils::BytesStdHash> subscriptions;
    std::vector<Message> publish;
  };

  class Topic {
   public:
    CoroOutcome<Bytes> receive();

    void publish(BytesIn message);

    std::weak_ptr<Gossip> weak_gossip_;
    TopicHash topic_hash_;
    CoroOutcomeChannel<Bytes> receive_channel_;
  };

  class Peer {
   public:
    Peer(PeerId peer_id);

    PeerId peer_id_;
    std::unordered_set<TopicHash, qtils::BytesStdHash> topics_;
    std::optional<StreamPtr> stream_out_;
    std::unordered_set<StreamPtr> streams_in_;
    std::optional<Rpc> batch_;
    bool writing_ = false;
  };

  class Gossip : public std::enable_shared_from_this<Gossip>,
                 public BaseProtocol {
   public:
    Gossip(std::shared_ptr<boost::asio::io_context> io_context,
           std::shared_ptr<host::BasicHost> host,
           std::shared_ptr<peer::IdentityManager> id_mgr,
           std::shared_ptr<crypto::CryptoProvider> crypto_provider,
           Config config);

    // Adaptor
    peer::ProtocolName getProtocolId() const override;

    // BaseProtocol
    void handle(StreamPtr stream) override;

    void start();

    std::shared_ptr<Topic> subscribe(TopicHash topic_hash);
    std::shared_ptr<Topic> subscribe(std::string_view topic_hash);

    void publish(Topic &topic, BytesIn data);

    void broadcast(std::optional<PeerId> from, const Message &message);

    bool onMessage(const std::shared_ptr<Peer> &peer, BytesIn encoded);

    std::shared_ptr<Peer> getPeer(const PeerId &peer_id);

    Rpc &getBatch(const std::shared_ptr<Peer> &peer);

    void checkWrite(const std::shared_ptr<Peer> &peer);

   private:
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<host::BasicHost> host_;
    std::shared_ptr<peer::IdentityManager> id_mgr_;
    std::shared_ptr<crypto::CryptoProvider> crypto_provider_;
    Config config_;
    event::Handle on_peer_sub_;
    std::unordered_map<TopicHash, std::shared_ptr<Topic>, qtils::BytesStdHash>
        topics_;
    std::unordered_map<PeerId, std::shared_ptr<Peer>> peers_;
    PublishConfigSigning publish_config_;
    DuplicateCache<MessageId, qtils::BytesStdHash> duplicate_cache_;
  };
}  // namespace libp2p::protocol::gossip
