#include "network.hpp"
#include <iostream>
#include <ostream>
#include <stdexcept>

void default_on_connect_callback() { std::cout << "connected to server" << std::endl; }

Network::Network(std::string ip_address, uint16_t port, OnConnectCallback on_connect_callback)
    : ip_address(std::move(ip_address)), port(port), on_connect_callback(std::move(on_connect_callback)),
      client(nullptr), peer(nullptr), recently_sent_packet_sizes(10), recently_sent_packet_times(10) {
    initialize_network();
}

Network::~Network() {
    if (client != nullptr) {
        enet_host_destroy(client);
    }
    enet_deinitialize();
}

float Network::average_bits_per_second_sent() {
    using namespace std::chrono;

    if (recently_sent_packet_sizes.empty() || recently_sent_packet_times.size() < 2) {
        return 0.0f; // Not enough data to calculate average
    }

    // Calculate the total size in bits and the time difference in seconds
    size_t total_size_bits = 0;
    for (const size_t size : recently_sent_packet_sizes) {
        total_size_bits += size * 8;
    }

    auto time_span =
        duration_cast<duration<float>>(recently_sent_packet_times.back() - recently_sent_packet_times.front());

    float total_time_seconds = time_span.count();
    if (total_time_seconds == 0.0f) {
        return 0.0f; // Avoid division by zero
    }

    return static_cast<float>(total_size_bits) / total_time_seconds; // Bits per second
}

void Network::set_server(std::string ip_address, uint16_t port) {
    this->ip_address = ip_address;
    this->port = port;
}

/**
 * /brief attempts to connect to the server specified in the constructor
 */
void Network::initialize_network() {
    LogSection _(global_logger, "initialize_network");
    if (enet_initialize() != 0) {
        global_logger.error("an error occurred while initializing ENet.");
        throw std::runtime_error("ENet initialization failed.");
    }

    client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (client == nullptr) {
        global_logger.error("an error occurred while trying to create an ENet client host.");
        throw std::runtime_error("ENet client host creation failed.");
    }

    global_logger.info("network initialized.");
}

/**
 * /brief attempts to connect to the server with ip address specified in the
 * constructor returns whether or not the connection was successful, this waits
 * for up to 5 seconds before stopping
 */
bool Network::attempt_to_connect_to_server() {
    LogSection _(global_logger, "attempt_to_connect_to_server");
    ENetAddress address;
    enet_address_set_host(&address, ip_address.c_str());
    address.port = port;

    peer = enet_host_connect(client, &address, 2, 0);
    if (peer == nullptr) {
        global_logger.error("No available peers for initiating an ENet connection.");
        return false;
    }

    ENetEvent event;
    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        global_logger.info("Connection to {}:{} succeeded.", ip_address, port);
        connected_to_server = true;
        return true;
    } else {
        enet_peer_reset(peer);
        global_logger.error("Connection to {}:{} failed.", ip_address, port);
        return false;
    }
}

/**
 * @brief collects nextwork events that occured since the last call to this
 * function /note users must implement PacketData which is a variant type of all
 * the possible packets the client can receive. /return the packets received
 * /author cuppajoeman
 */
std::vector<PacketWithSize> Network::get_network_events_received_since_last_tick() {
    LogSection _(global_logger, "get_network_events_received_since_last_tick");

    if (not connected_to_server) {
        global_logger.warn("not connected to server");
        return {};
    }

    ENetEvent event;
    std::vector<PacketWithSize> received_packets;

    PacketWithSize packet_with_size;

    while (enet_host_service(client, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_RECEIVE:
            global_logger.info("Packet received from peer {}: size {} bytes.", event.peer->address.host,
                               event.packet->dataLength);

            packet_with_size.data.resize(event.packet->dataLength);

            std::memcpy(packet_with_size.data.data(), event.packet->data, event.packet->dataLength);
            packet_with_size.size = event.packet->dataLength;

            received_packets.push_back(packet_with_size);

            enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            global_logger.info("Peer {} disconnected.", event.peer->address.host);
            event.peer->data = nullptr;
            break;

        default:
            break;
        }
    }

    return received_packets;
}

/**
 * /pre the client is currently connected to a server
 * /brief disconnect from the server, waiting up to 3 seconds for a responce
 * about the progress of the disconnection before giving up.
 */
void Network::disconnect_from_server() {
    if (peer != nullptr) {
        enet_peer_disconnect(peer, 0);
        ENetEvent event;
        while (enet_host_service(client, &event, 3000) > 0) {
            if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                global_logger.info("Disconnection succeeded.");
                peer = nullptr;
                break;
            }
        }
    }
}

/**
 * /pre we are connected to the server
 * /brief attempts to send a packet to the server
 * /param data a pointer to the data
 * /param data_size how many bytes of data to send from the given memory address
 * /param reliable whether or not to send the packet reliably
 */
void Network::send_packet(const void *data, size_t data_size, bool reliable) {
    LogSection _(global_logger, "send_packet");

    if (not connected_to_server) {
        global_logger.warn("not connected to server can't send the packet");
        return;
    }
    // global_logger.info("Sending packet to server");
    ENetPacket *packet = enet_packet_create(data, data_size, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(peer->host);
    global_logger.debug("just sent the packet");
    recently_sent_packet_sizes.push_back(packet->dataLength);
    recently_sent_packet_times.push_back(std::chrono::steady_clock::now());
}
