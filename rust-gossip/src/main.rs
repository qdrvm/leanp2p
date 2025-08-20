use libp2p::Multiaddr;
use libp2p::PeerId;
use libp2p::futures::StreamExt;
use libp2p::gossipsub;
use libp2p::identity::Keypair;
use libp2p::swarm::NetworkBehaviour;
use libp2p::swarm::SwarmEvent;

mod stdio;

struct SamplePeer {
    index: usize,
    keypair: Keypair,
    listen: Multiaddr,
    connect: Multiaddr,
}
impl SamplePeer {
    fn new(index: usize) -> Self {
        let port = 10000 + index;
        let mut private_key = [0u8; 32];
        for i in 0..32 {
            private_key[i] = (i + index) as _;
        }
        let keypair = Keypair::ed25519_from_bytes(private_key).unwrap();
        let peer_id = PeerId::from_public_key(&keypair.public());
        let listen = format!("/ip4/127.0.0.1/udp/{port}/quic-v1")
            .parse()
            .unwrap();
        let connect = format!("{listen}/p2p/{peer_id}").parse().unwrap();
        Self {
            index,
            keypair,
            listen,
            connect,
        }
    }
}

#[derive(NetworkBehaviour)]
pub struct Behaviour {
    pub gossip: gossipsub::Behaviour,
}

#[tokio::main(flavor = "current_thread")]
async fn main() -> anyhow::Result<()> {
    _ = tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .try_init();
    let args: Vec<_> = std::env::args()
        .skip(1)
        .map(|s| s.parse::<usize>().unwrap())
        .map(SamplePeer::new)
        .collect();
    if args.len() < 1 {
        println!("usage: rust-gossip [index to listen] [indices to connect]...");
        return Ok(());
    }

    let mut stdin = stdio::Stdin::new()?;
    let config = gossipsub::ConfigBuilder::default()
        .support_floodsub()
        .build()
        .unwrap();
    let mut swarm = libp2p::SwarmBuilder::with_existing_identity(args[0].keypair.clone())
        .with_tokio()
        .with_quic()
        .with_behaviour(|key| Behaviour {
            gossip: gossipsub::Behaviour::new(
                gossipsub::MessageAuthenticity::Signed(key.clone()),
                config.clone(),
            )
            .unwrap(),
        })
        .unwrap()
        .build();
    println!("LocalPeerId {}", swarm.local_peer_id());
    swarm.listen_on(args[0].listen.clone()).unwrap();
    for arg in &args[1..] {
        println!("connect to {}", arg.index);
        swarm.dial(arg.connect.clone()).unwrap();
    }
    let topic = gossipsub::IdentTopic::new("example");
    swarm.behaviour_mut().gossip.subscribe(&topic).unwrap();
    loop {
        tokio::select! {
            line = stdin.read() => match line {
                Ok(line) => {
                    if !line.is_empty() {
                        swarm.behaviour_mut().gossip.publish(topic.hash(), format!("{}: {line}", args[0].index)).unwrap();
                    }
                },
                Err(_) => break,
            },
            event = swarm.select_next_some() => match event {
                SwarmEvent::Behaviour(BehaviourEvent::Gossip(event)) => match event {
                    gossipsub::Event::Message { message, .. } => {
                        println!("chat {}", std::str::from_utf8(&message.data).unwrap());
                    }
                    gossipsub::Event::Subscribed { peer_id, topic } => {
                        println!("Subscribed {peer_id} {}", topic.as_str());
                    }
                    gossipsub::Event::GossipsubNotSupported { peer_id, .. } => {
                        println!("GossipsubNotSupported {peer_id}");
                    }
                    _ => {}
                },
                SwarmEvent::ConnectionEstablished { peer_id, .. } => {
                    println!("ConnectionEstablished {peer_id}")
                }
                SwarmEvent::ConnectionClosed { peer_id, cause, .. } => {
                    println!("ConnectionClosed {peer_id} {cause:?}")
                }
                SwarmEvent::IncomingConnection { .. } => println!("IncomingConnection"),
                SwarmEvent::IncomingConnectionError { error, peer_id, .. } => {
                    println!("IncomingConnectionError {peer_id:?} {error}")
                }
                SwarmEvent::OutgoingConnectionError { peer_id, error, .. } => {
                    println!("OutgoingConnectionError {peer_id:?} {error}")
                }
                _ => {}
            }
        }
    }
    Ok(())
}
