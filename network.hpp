#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <enet/enet.h>
#include <spdlog/spdlog.h>
#include <functional>
#include <string>

class Network {
public:
    using PacketCallback = std::function<void(ENetPacket *)>;

    Network(const std::string &ip_address, uint16_t port, PacketCallback packet_callback);

    ~Network();

    void initialize_network();

    bool attempt_to_connect_to_server();

    void process_network_events_received_since_last_tick();

    void disconnect_from_server();

private:
    std::string ip_address;
    uint16_t port;
    PacketCallback packet_callback;
    ENetHost *client;
    ENetPeer *peer;
};

#endif // NETWORK_HPP
