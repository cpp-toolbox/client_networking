#include "network.hpp"
#include <stdexcept>

Network::Network(const std::string &ip_address, uint16_t port, PacketCallback packet_callback)
        : ip_address(ip_address), port(port), packet_callback(packet_callback), client(nullptr), peer(nullptr) {}

Network::~Network() {
    if (client != nullptr) {
        enet_host_destroy(client);
    }
    enet_deinitialize();
}

void Network::initialize_network() {
    if (enet_initialize() != 0) {
        spdlog::error("An error occurred while initializing ENet.");
        throw std::runtime_error("ENet initialization failed.");
    }

    client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (client == nullptr) {
        spdlog::error("An error occurred while trying to create an ENet client host.");
        throw std::runtime_error("ENet client host creation failed.");
    }

    spdlog::info("Network initialized.");
}

bool Network::attempt_to_connect_to_server() {
    ENetAddress address;
    enet_address_set_host(&address, ip_address.c_str());
    address.port = port;

    peer = enet_host_connect(client, &address, 2, 0);
    if (peer == nullptr) {
        spdlog::error("No available peers for initiating an ENet connection.");
        return false;
    }

    ENetEvent event;
    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        spdlog::info("Connection to {}:{} succeeded.", ip_address, port);
        return true;
    } else {
        enet_peer_reset(peer);
        spdlog::error("Connection to {}:{} failed.", ip_address, port);
        return false;
    }
}

void Network::process_network_events_received_since_last_tick() {
    ENetEvent event;
    while (enet_host_service(client, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                spdlog::info("Packet received from peer {}.", event.peer->address.host);
                packet_callback(event.packet);
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                spdlog::info("Peer {} disconnected.", event.peer->address.host);
                event.peer->data = nullptr;
                break;

            default:
                break;
        }
    }
}

void Network::disconnect_from_server() {
    if (peer != nullptr) {
        enet_peer_disconnect(peer, 0);
        ENetEvent event;
        while (enet_host_service(client, &event, 3000) > 0) {
            if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                spdlog::info("Disconnection succeeded.");
                peer = nullptr;
                break;
            }
        }
    }
}
