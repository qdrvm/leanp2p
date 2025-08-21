#include <libp2p/multi/multiaddress.hpp>
#include <print>

int main() {
  auto address = libp2p::multi::Multiaddress::create("/ip4/127.0.0.1/tcp/8080");
  std::println("Created multiaddress: {}", address);
}
