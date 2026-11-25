#pragma once
#include "audio/sfx.h"
#include "core/scene.h"
#include "game/world.h"
#include "net/session.h"
#include "render/effects.h"
#include "render/scene_renderer.h"
#include "ui/evo_ui.h"

#include "compiled/warning_off.h"
#include <deque>
#include <string>
#include "compiled/warning_on.h"

namespace evo
{
constexpr Float FEED_SECONDS = 6.0f;

[[nodiscard]]
FVec3 PickupColor(PickupKind kind);

enum class Screen : UInt8
{
    MAIN_MENU,
    CONNECTING,
    IN_MATCH
};

struct Options
{
    std::string name = "Player";
    Float sensitivity = 1.0f;
    Float fieldOfView = 100.0f;
    Float volume = 0.8f;
    Bool invertY = false;
    Bool gore = true;
    MatchSettings match;
};

struct InputFrame
{
    Float lookX = 0.0f;
    Float lookY = 0.0f;
    Float forward = 0.0f;
    Float right = 0.0f;
    Bool jump = false;
    DodgeDirection dodge = DodgeDirection::NONE;
    Bool fireHeld = false;
    Bool firePressed = false;
    int selectWeapon = -1;
    Bool throwGrenade = false;
    Bool placeMine = false;
    int cycleWeapon = 0;
    Bool lastWeapon = false;
    Bool scoreboard = false;
    Bool escape = false;
};

class GameScene final : public core::Scene
{
  public:
    GameScene();
    ~GameScene() override;
    void Update(double elapsed_seconds) override;
    void Render() override;

  private:
    struct FeedEntry
    {
        int killer = -1;
        int victim = -1;
        std::string killerName;
        std::string victimName;
        FVec3 killerColor;
        FVec3 victimColor;
        DamageSource weapon = DamageSource::RIFLE;
        Bool headshot = false;
        Float time = 0.0f;
    };

    struct Banner
    {
        std::string text;
        std::string subtext;
        FVec3 color{1.0f, 1.0f, 1.0f};
        Float until = 0.0f;
    };

    void InitializeUi();
    void LoadOptions();
    void SaveOptions() const;
    InputFrame ReadInput(Float delta_seconds);
    void StartDemo();
    void SelectArena(ArenaId arena);
    void StartOffline();
    void StartHost();
    void StartJoin();
    void LeaveMatch(const std::string &reason);
    void EnterMatch();
    void UpdateMatch(Float delta_seconds, const InputFrame &input);
    void UpdateLocalPlayerHost(Float delta_seconds, const InputFrame &input);
    void UpdateLocalPlayerClient(Float delta_seconds, const InputFrame &input);
    void ApplyLook(Player &player, const InputFrame &input) const;
    void HandleWeaponInput(Player &player, const InputFrame &input);
    void SelectWeapon(Player &player, WeaponId weapon);
    Bool AutoSwitchIfEmpty(Player &player);
    void ProcessEvents();
    void HandleEvent(const GameEvent &event);
    void DrawGrenade(const FVec3 &center, Float spin, const FVec3 &light, Float blink);
    void DrawMine(const FVec3 &center, Float spin, const FVec3 &light, Float blink);
    void HandleShot(int shooter, WeaponId weapon, const FVec3 &origin, const FVec3 &end, UInt8 pellet_seed);
    void PlayLocalMoveSounds();
    void UpdateRenderState(Float delta_seconds);
    void UpdateCamera(Float delta_seconds);
    void UpdateCapture(const InputFrame &input);
    [[nodiscard]]
    Bool IsLocal(int player_id) const;
    [[nodiscard]]
    const Player *LocalPlayer() const;
    [[nodiscard]]
    FVec3 ViewmodelMuzzle() const;
    [[nodiscard]]
    std::string PlayerName(int player_id) const;
    [[nodiscard]]
    std::vector<int> RankedPlayers() const;

    void DrawWorld();
    void DrawPlayer(const Player &player);
    void DrawWeaponModel(WeaponId weapon, const FVec3 &position, Float yaw, Float pitch, Float scale);
    void DrawPickups();
    void DrawJumpPads();
    void DrawProjectiles();
    void DrawViewmodel();

    // The menus and the HUD live in resources/ui/evo.rml; these fill in its state.
    void UpdateUi();
    void FillHud();
    // Copies what the player changed in the menus into the options; true if the forms need a refresh.
    Bool SyncUiForms();
    [[nodiscard]]
    Bool IsTypingInUi() const;
    [[nodiscard]]
    Bool IsPointerOverUi() const;

    World world;
    net::Session session;
    SceneRenderer renderer;
    Effects effects;
    Sfx sfx;
    Options options;
    Screen screen = Screen::MAIN_MENU;
    Bool rendererReady = false;
    Bool uiReady = false;
    Bool menuOpen = false;
    Bool wasCaptured = false;
    Bool fireArmed = false;
    Bool showScoreboard = false;
    Bool autoStartDone = false;
    std::string errorMessage;
    std::string joinAddress;
    std::string hostAddress;
    std::string roomCode;
    std::array<Bool, 24> previousKeys{};
    std::array<Float, 4> lastTapTime{};
    Bool previousFire = false;
    Bool previousAltFire = false;
    // Thrown things leave a trail every other frame.
    Bool effectsTrailToggle = false;
    Float wheel = 0.0f;
    Float realTime = 0.0f;
    Float shake = 0.0f;
    Float recoil = 0.0f;
    Float landDip = 0.0f;
    Float bobPhase = 0.0f;
    Float hitMarker = 0.0f;
    Float damageFlash = 0.0f;
    Float respawnRequestTimer = 0.0f;
    WeaponId previousWeapon = WeaponId::RIFLE;
    FVec3 cameraEye;
    FVec3 cameraTilt;
    Float cameraYaw = 0.0f;
    Float cameraPitch = 0.0f;
    FVec3 deathPosition;
    Float deathYaw = 0.0f;
    Float deathStartTime = 0.0f;
    std::deque<FeedEntry> feed;
    std::deque<std::pair<std::string, Float>> notices;
    Banner banner;
    Banner pickupBanner;
    EvoUi ui;
};
} // namespace evo
