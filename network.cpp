#include "network.hpp"
#include <stdexcept>

void default_on_connect_callback() { spdlog::info("connected to server"); }

Network::Network(std::string ip_address, uint16_t port, const std::vector<spdlog::sink_ptr> &sinks,
                 OnConnectCallback on_connect_callback)
    : ip_address(std::move(ip_address)), port(port), on_connect_callback(std::move(on_connect_callback)),
      client(nullptr), peer(nullptr) {
    logger_component = LoggerComponent("network", sinks);
}

Network::~Network() {
    if (client != nullptr) {
        enet_host_destroy(client);
    }
    enet_deinitialize();
}

/**
 * /brief attempts to connect to the server specified in the constructor
 */
void Network::initialize_network() {
    if (enet_initialize() != 0) {
        if (logger_component.logging_enabled) {
            logger_component.get_logger()->error("An error occurred while initializing ENet.");
        }
        throw std::runtime_error("ENet initialization failed.");
    }

    client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (client == nullptr) {
        if (logger_component.logging_enabled) {
            logger_component.get_logger()->error("An error occurred while trying to create an ENet client host.");
        }
        throw std::runtime_error("ENet client host creation failed.");
    }

    if (logger_component.logging_enabled) {
        logger_component.get_logger()->info("Network initialized.");
    }
}

/**
 * /brief attempts to connect to the server with ip address specified in the
 * constructor /return whether or not the connection was successful /author
 * cuppajoeman
 */
bool Network::attempt_to_connect_to_server() {
    ENetAddress address;
    enet_address_set_host(&address, ip_address.c_str());
    address.port = port;

    peer = enet_host_connect(client, &address, 2, 0);
    if (peer == nullptr) {
        if (logger_component.logging_enabled) {
            logger_component.get_logger()->error("No available peers for initiating an ENet connection.");
        }
        return false;
    }

    ENetEvent event;
    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        if (logger_component.logging_enabled) {
            logger_component.get_logger()->info("Connection to {}:{} succeeded.", ip_address, port);
        }
        return true;
    } else {
        enet_peer_reset(peer);
        if (logger_component.logging_enabled) {
            logger_component.get_logger()->error("Connection to {}:{} failed.", ip_address, port);
        }
        return false;
    }
}

/**
 * @brief collects nextwork events that occured since the last call to this
 * function /note users must implement PacketData which is a variant type of all
 * the possible packets the client can receive. /return the packets received
 * /author cuppajoeman
 */
std::vector<void *> Network::get_network_events_received_since_last_tick() {
    ENetEvent event;
    std::vector<void *> received_packets;

    while (enet_host_service(client, &event, 0) > 0) {
        switch (event.type) {

        case ENET_EVENT_TYPE_RECEIVE:
            if (logger_component.logging_enabled) {
                logger_component.get_logger()->info("Packet received from peer {}.", event.peer->address.host);
            }
            received_packets.push_back(event.packet);
            enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            if (logger_component.logging_enabled) {
                logger_component.get_logger()->info("Peer {} disconnected.", event.peer->address.host);
            }
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

                if (logger_component.logging_enabled) {
                    logger_component.get_logger()->info("Disconnection succeeded.");
                }
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

    if (logger_component.logging_enabled) {
        logger_component.get_logger()->info("Sending packet to server");
    }
    ENetPacket *packet = enet_packet_create(data, data_size, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(peer->host);
}
