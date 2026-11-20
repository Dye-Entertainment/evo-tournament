#ifdef __EMSCRIPTEN__
#include "net/transport.h"

#include "compiled/warning_off.h"
#include <array>
#include <emscripten.h>
#include "compiled/warning_on.h"

extern "C"
{
int evo_net_host(const char *room);
int evo_net_join(const char *room);
void evo_net_send(int peer, const unsigned char *data, int length);
int evo_net_poll(unsigned char *buffer, int capacity, int *peer, int *type);
void evo_net_status(char *buffer, int capacity);
int evo_net_ready();
void evo_net_shutdown();
}

namespace evo::net
{
namespace
{
constexpr int MAX_MESSAGE = 65536;

class WebRtcTransport final : public Transport
{
  public:
    ~WebRtcTransport() override
    {
        Shutdown();
    }

    Bool StartHost(const std::string &address) override
    {
        return evo_net_host(address.c_str()) != 0;
    }

    Bool StartClient(const std::string &address) override
    {
        return evo_net_join(address.c_str()) != 0;
    }

    void Send(const int peer, const std::vector<UInt8> &data) override
    {
        evo_net_send(peer, data.data(), static_cast<int>(data.size()));
    }

    Bool Poll(TransportEvent &event) override
    {
        int peer = -1;
        int type = 0;
        const int length = evo_net_poll(buffer.data(), MAX_MESSAGE, &peer, &type);
        if (length < 0)
        {
            return false;
        }
        event.peer = peer;
        event.type = static_cast<TransportEventType>(type);
        event.data.assign(buffer.begin(), buffer.begin() + length);
        return true;
    }

    void Shutdown() override
    {
        evo_net_shutdown();
    }

    [[nodiscard]]
    Bool IsReady() const override
    {
        return evo_net_ready() != 0;
    }

    [[nodiscard]]
    std::string Status() const override
    {
        std::array<char, 256> text{};
        evo_net_status(text.data(), static_cast<int>(text.size()));
        return std::string(text.data());
    }

  private:
    std::array<UInt8, MAX_MESSAGE> buffer{};
};
} // namespace

std::unique_ptr<Transport> CreateTransport()
{
    return std::make_unique<WebRtcTransport>();
}

Bool UsesRoomCodes()
{
    return true;
}

std::string DefaultHostAddress()
{
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string code;
    for (int i = 0; i < 5; ++i)
    {
        code += alphabet[static_cast<int>(emscripten_random() * 32.0f) % 32];
    }
    return code;
}
} // namespace evo::net
#endif
