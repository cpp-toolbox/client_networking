#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <enet/enet.h>
#include <functional>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

#include "sbpt_generated_includes.hpp"

// Default packet callback that logs receipt of packets
void default_on_connect_callback();

using OnConnectCallback = std::function<void()>;

class Network {
  public:
    Network(std::string ip_address, uint16_t port, OnConnectCallback on_connect_callback = default_on_connect_callback)
        : ip_address(std::move(ip_address)), port(port), on_connect_callback(std::move(on_connect_callback)),
          client(nullptr), peer(nullptr){};

    ~Network();

    void initialize_network();

    bool attempt_to_connect_to_server();

    std::vector<PacketData> get_network_events_received_since_last_tick();

    void disconnect_from_server();

    void send_packet(const void *data, size_t data_size, bool reliable = false);

  private:
    std::string ip_address;
    uint16_t port;
    OnConnectCallback on_connect_callback;
    ENetHost *client;
    ENetPeer *peer;
};

#endif // NETWORK_HPP
