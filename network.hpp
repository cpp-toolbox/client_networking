#ifndef CLIENT_NETWORK_HPP
#define CLIENT_NETWORK_HPP

#include <enet/enet.h>
#include <functional>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

#include "sbpt_generated_includes.hpp"

#include <cstddef> // for size_t

struct PacketWithSize {
    std::vector<char> data;  // Use std::vector<char> to hold packet data
    size_t size;             // Size of the packet data
};

// Default packet callback that logs receipt of packets
void default_on_connect_callback();


using OnConnectCallback = std::function<void()>;

class Network {
  public:
    Network(std::string ip_address, uint16_t port, const std::vector<spdlog::sink_ptr> &sinks = {},
            OnConnectCallback on_connect_callback = default_on_connect_callback);
    ~Network();
    LoggerComponent logger_component;
    void initialize_network();
    bool attempt_to_connect_to_server();
    std::vector<PacketWithSize> get_network_events_received_since_last_tick();
    void disconnect_from_server();
    void send_packet(const void *data, size_t data_size, bool reliable = false);

  private:
    std::string ip_address;
    uint16_t port;
    OnConnectCallback on_connect_callback;
    ENetHost *client;
    ENetPeer *peer;
};

#endif // CLIENT_NETWORK_HPP
