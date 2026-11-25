#pragma once
#include "platform/types.h"

#include "compiled/warning_off.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "compiled/warning_on.h"

namespace Rml
{
class ElementDocument;
}

namespace evo
{
struct UiFeedEntry
{
    std::string killer;
    std::string killerColor = "#ffffff";
    std::string weapon;
    std::string victim;
    std::string victimColor = "#ffffff";
    Float alpha = 1.0f;
    Bool involved = false;
    Bool suicide = false;
};

struct UiNameplate
{
    std::string name;
    std::string color = "#ffffff";
    Float x = 0.0f;
    Float y = 0.0f;
    Float size = 16.0f;
    Float alpha = 1.0f;
};

struct UiWeaponSlot
{
    std::string label;
    std::string ammo;
    std::string color = "#ffffff";
    Bool owned = false;
    Bool selected = false;
};

struct UiNotice
{
    std::string text;
    Float alpha = 1.0f;
};

struct UiScoreRow
{
    Int32 rank = 0;
    std::string name;
    std::string color = "#ffffff";
    std::string tag;
    Int32 frags = 0;
    Int32 deaths = 0;
    Bool local = false;
};

// Everything resources/ui/evo.rml shows. The game fills it in every frame; the document reads
// it through an RmlUi data model. The form fields (name, sliders, text boxes) are two-way: the
// document writes them back when the player edits them.
struct UiState
{
    std::string screen = "menu"; // menu, connecting or match

    // Form fields
    std::string name;
    Int32 botCount = 4;
    Float botSkill = 0.3f;
    Int32 fragLimit = 15;
    Int32 timeLimit = 8;
    Float sensitivity = 1.0f;
    Float fov = 100.0f;
    Float volume = 0.8f;
    Bool invertY = false;
    Bool gore = true;
    std::string hostPort;
    std::string joinText;
    Bool showMatch = false;
    Bool showOptions = false;
    Bool showControls = false;
    Bool showPauseOptions = false;

    // Main menu
    Int32 arena = 0;
    std::string arenaDescription;
    std::string skillName;
    Bool roomCodes = false;
    std::string onlineHelp;

    // Connecting and errors
    std::string connectStatus;
    std::string error;

    // HUD
    Float damage = 0.0f;
    Bool alive = false;
    std::string crosshair; // rifle, nova or rail
    Float hit = 0.0f;
    Int32 health = 0;
    Int32 armor = 0;
    Bool healthLow = false;
    Float healthAlpha = 1.0f;
    std::string weaponName;
    std::string weaponColor = "#ffffff";
    Int32 ammo = 0;
    std::vector<UiWeaponSlot> slots;
    Bool respawn = false;
    Float respawnAlpha = 1.0f;
    std::string timer;
    Bool timerLow = false;
    Int32 matchFragLimit = 0;
    Bool hasRank = false;
    std::string rank;
    Bool rankFirst = false;
    std::string rankOf;
    std::string fragsText;
    std::string lead;
    Bool leadGood = false;
    std::vector<UiFeedEntry> feed;
    std::vector<UiNameplate> nameplates;
    std::string status;
    std::vector<UiNotice> notices;
    std::string banner;
    std::string bannerSub;
    std::string bannerColor = "#ffffff";
    Float bannerAlpha = 0.0f;
    std::string pickup;
    std::string pickupColor = "#ffffff";
    Float pickupAlpha = 0.0f;
    Bool scoreboard = false;
    std::string scoreTitle;
    std::vector<UiScoreRow> scores;

    // Pause menu
    Bool pause = false;
    std::string pauseTitle;
    Bool isHost = false;
    std::string roomCodeText;
    std::string inviteLink;
    std::string friends;
    std::string connectedTo;
    Bool showBots = false;
    Bool showResume = false;
};

class EvoUi final
{
  public:
    Bool Initialize(const std::string &ui_dir);
    void Shutdown();

    UiState &State()
    {
        return state;
    }
    // Runs `callback` when the document fires action('<name>') (buttons).
    void OnAction(const std::string &name, std::function<void()> callback);
    // Sends the game-side state to the document. Form fields are only sent when
    // `forms_changed` is set, so the game does not fight the player's typing.
    void Commit(Bool forms_changed);
    // The UI scale: 1dp in the document is this many window pixels.
    [[nodiscard]] Float Scale() const
    {
        return scale;
    }

  private:
    UiState state;
    Rml::ElementDocument *document = nullptr;
    std::unordered_map<std::string, std::function<void()>> actions;
    std::vector<std::string> displayVariables;
    std::vector<std::string> formVariables;
    Float scale = 1.0f;
    Bool ready = false;
};
} // namespace evo
