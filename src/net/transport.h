#pragma once
#include "platform/types.h"

#include "compiled/warning_off.h"
#include <memory>
#include <string>
#include <vector>
#include "compiled/warning_on.h"

namespace evo::net
{
enum class TransportEventType : UInt8
{
    CONNECTED,
    DISCONNECTED,
    DATA
};

struct TransportEvent
{
    TransportEventType type = TransportEventType::DATA;
    int peer = -1;
    std::vector<UInt8> data;
};

class Transport
{
  public:
    virtual ~Transport() = default;
    virtual Bool StartHost(const std::string &address) = 0;
    virtual Bool StartClient(const std::string &address) = 0;
    virtual void Send(int peer, const std::vector<UInt8> &data) = 0;
    virtual Bool Poll(TransportEvent &event) = 0;
    virtual void Shutdown() = 0;
    [[nodiscard]]
    virtual Bool IsReady() const = 0;
    [[nodiscard]]
    virtual std::string Status() const = 0;
};

std::unique_ptr<Transport> CreateTransport();

[[nodiscard]]
Bool UsesRoomCodes();

[[nodiscard]]
std::string DefaultHostAddress();
} // namespace evo::net
