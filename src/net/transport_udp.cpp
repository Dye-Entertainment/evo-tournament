#ifndef __EMSCRIPTEN__
#include "net/transport.h"

#include "compiled/warning_off.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "compiled/warning_on.h"

namespace evo::net
{
namespace
{
constexpr UInt16 DEFAULT_PORT = 27777;
constexpr double PEER_TIMEOUT_SECONDS = 10.0;
constexpr size_t MAX_DATAGRAM = 65507;

double Now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Bool SameAddress(const sockaddr_in &a, const sockaddr_in &b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

class UdpTransport final : public Transport
{
  public:
    ~UdpTransport() override
    {
        Shutdown();
    }

    Bool StartHost(const std::string &address) override
    {
        Shutdown();
        const int port = address.empty() ? DEFAULT_PORT : std::atoi(address.c_str());
        if (!OpenSocket())
        {
            return false;
        }
        sockaddr_in bind_address{};
        bind_address.sin_family = AF_INET;
        bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
        bind_address.sin_port = htons(static_cast<UInt16>(port));
        if (bind(socketHandle, reinterpret_cast<sockaddr *>(&bind_address), sizeof(bind_address)) != 0)
        {
            status = "Could not bind UDP port " + std::to_string(port);
            Shutdown();
            return false;
        }
        isHost = true;
        ready = true;
        status = "Hosting on UDP port " + std::to_string(port);
        return true;
    }

    Bool StartClient(const std::string &address) override
    {
        Shutdown();
        std::string host = address;
        std::string port = std::to_string(DEFAULT_PORT);
        if (const size_t colon = address.rfind(':'); colon != std::string::npos)
        {
            host = address.substr(0, colon);
            port = address.substr(colon + 1);
        }
        if (host.empty())
        {
            host = "127.0.0.1";
        }
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo *result = nullptr;
        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0 || result == nullptr)
        {
            status = "Could not resolve " + host;
            return false;
        }
        Peer server{};
        std::memcpy(&server.address, result->ai_addr, sizeof(sockaddr_in));
        freeaddrinfo(result);
        if (!OpenSocket())
        {
            return false;
        }
        server.lastHeard = Now();
        peers.push_back(server);
        isHost = false;
        ready = true;
        status = "Connecting to " + host + ":" + port;
        events.push_back(TransportEvent{TransportEventType::CONNECTED, 0, {}});
        return true;
    }

    void Send(const int peer, const std::vector<UInt8> &data) override
    {
        if (socketHandle < 0 || peer < 0 || peer >= static_cast<int>(peers.size()) ||
            !peers[peer].active)
        {
            return;
        }
        sendto(socketHandle, data.data(), data.size(), 0,
               reinterpret_cast<const sockaddr *>(&peers[peer].address), sizeof(sockaddr_in));
    }

    Bool Poll(TransportEvent &event) override
    {
        Receive();
        CheckTimeouts();
        if (events.empty())
        {
            return false;
        }
        event = std::move(events.front());
        events.pop_front();
        return true;
    }

    void Shutdown() override
    {
        if (socketHandle >= 0)
        {
            close(socketHandle);
        }
        socketHandle = -1;
        peers.clear();
        events.clear();
        ready = false;
    }

    [[nodiscard]]
    Bool IsReady() const override
    {
        return ready;
    }

    [[nodiscard]]
    std::string Status() const override
    {
        return status;
    }

  private:
    struct Peer
    {
        sockaddr_in address{};
        double lastHeard = 0.0;
        Bool active = true;
    };

    Bool OpenSocket()
    {
        socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketHandle < 0)
        {
            status = "Could not create UDP socket";
            return false;
        }
        const int flags = fcntl(socketHandle, F_GETFL, 0);
        fcntl(socketHandle, F_SETFL, flags | O_NONBLOCK);
        return true;
    }

    void Receive()
    {
        if (socketHandle < 0)
        {
            return;
        }
        buffer.resize(MAX_DATAGRAM);
        while (true)
        {
            sockaddr_in from{};
            socklen_t from_length = sizeof(from);
            const ssize_t received = recvfrom(socketHandle, buffer.data(), buffer.size(), 0,
                                              reinterpret_cast<sockaddr *>(&from), &from_length);
            if (received <= 0)
            {
                return;
            }
            int peer_index = -1;
            for (int i = 0; i < static_cast<int>(peers.size()); ++i)
            {
                if (peers[i].active && SameAddress(peers[i].address, from))
                {
                    peer_index = i;
                    break;
                }
            }
            if (peer_index < 0)
            {
                if (!isHost)
                {
                    continue;
                }
                peers.push_back(Peer{from, Now(), true});
                peer_index = static_cast<int>(peers.size()) - 1;
                events.push_back(TransportEvent{TransportEventType::CONNECTED, peer_index, {}});
            }
            peers[peer_index].lastHeard = Now();
            events.push_back(TransportEvent{
                TransportEventType::DATA, peer_index,
                std::vector<UInt8>(buffer.begin(), buffer.begin() + received)
            });
        }
    }

    void CheckTimeouts()
    {
        const double now = Now();
        for (int i = 0; i < static_cast<int>(peers.size()); ++i)
        {
            if (peers[i].active && now - peers[i].lastHeard > PEER_TIMEOUT_SECONDS)
            {
                peers[i].active = false;
                events.push_back(TransportEvent{TransportEventType::DISCONNECTED, i, {}});
            }
        }
    }

    int socketHandle = -1;
    Bool isHost = false;
    Bool ready = false;
    std::string status = "Idle";
    std::vector<Peer> peers;
    std::deque<TransportEvent> events;
    std::vector<UInt8> buffer;
};
} // namespace

std::unique_ptr<Transport> CreateTransport()
{
    return std::make_unique<UdpTransport>();
}

Bool UsesRoomCodes()
{
    return false;
}

std::string DefaultHostAddress()
{
    return std::to_string(DEFAULT_PORT);
}
} // namespace evo::net
#endif
