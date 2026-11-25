#pragma once
#include "game/world.h"
#include "net/byte_stream.h"
#include "net/transport.h"

#include "compiled/warning_off.h"
#include <memory>
#include <string>
#include <vector>
#include "compiled/warning_on.h"

namespace evo::net
{
using ::net::ByteReader;
using ::net::ByteWriter;
using ::net::Transport;
using ::net::TransportEvent;
using ::net::TransportEventType;
using ::net::UsesRoomCodes;

constexpr UInt8 PROTOCOL_VERSION = 2;

[[nodiscard]]
const ::net::TransportSettings &GetTransportSettings();

[[nodiscard]]
std::string DefaultHostAddress();

enum class MessageType : UInt8
{
    HELLO = 1,
    WELCOME,
    REJECT,
    SNAPSHOT,
    ROSTER,
    CLIENT_STATE = 10,
    CLIENT_FIRE,
    CLIENT_RESPAWN,
    BYE
};

enum class SessionMode : UInt8
{
    NONE,
    OFFLINE,
    HOST,
    CLIENT
};

enum class SessionState : UInt8
{
    IDLE,
    CONNECTING,
    ACTIVE,
    FAILED
};

class Session final
{
  public:
    void StartOffline(World &world, const MatchSettings &settings, const std::string &name);
    Bool StartHost(World &world, const MatchSettings &settings, const std::string &name,
                   const std::string &address);
    Bool StartClient(World &world, const std::string &name, const std::string &address);
    void Update(World &world, Float delta_seconds);
    void SendFire(WeaponId weapon, const FVec3 &origin, const FVec3 &direction);
    void SendRespawnRequest();
    // Host and offline only: adds or removes bots to reach `count` and applies `skill` to all of them.
    void SetBots(World &world, int count, Float skill);
    void Shutdown(World &world);

    [[nodiscard]]
    std::string TransportStatus() const;
    [[nodiscard]]
    int RemotePlayerCount(const World &world) const;
    std::vector<std::string> TakeMessages();

    SessionMode mode = SessionMode::NONE;
    SessionState state = SessionState::IDLE;
    int localPlayer = -1;
    std::string address;
    std::string failureReason;

  private:
    void HostUpdate(World &world, Float delta_seconds);
    void ClientUpdate(World &world, Float delta_seconds);
    void HostReceive(World &world, int peer, const std::vector<UInt8> &data);
    void ClientReceive(World &world, const std::vector<UInt8> &data);
    void HostRemovePeer(World &world, int peer);
    void FillBots(World &world);
    void SendToRemotes(const World &world, const std::vector<UInt8> &data);
    [[nodiscard]]
    std::vector<UInt8> BuildSnapshot(const World &world) const;
    [[nodiscard]]
    std::vector<UInt8> BuildRoster(const World &world) const;
    void ApplySnapshot(World &world, const std::vector<UInt8> &data);
    void ApplyRoster(World &world, const std::vector<UInt8> &data);
    void Fail(const std::string &reason);

    std::unique_ptr<Transport> transport;
    std::string playerName;
    Float snapshotTimer = 0.0f;
    Float rosterTimer = 0.0f;
    Float helloTimer = 0.0f;
    Float stateTimer = 0.0f;
    Float connectTimer = 0.0f;
    Float silenceTimer = 0.0f;
    Bool transportConnected = false;
    Bool rosterDirty = false;
    Bool hasEventSequence = false;
    UInt16 lastEventSequence = 0;
    std::vector<std::string> messages;
    std::array<std::string, MAX_PLAYERS> knownNames;
};
} // namespace evo::net
