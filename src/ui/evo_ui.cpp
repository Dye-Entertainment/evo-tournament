#include "ui/evo_ui.h"

#include "ui/ui_manager.h"
#include "utils/logger.h"
#include "window/window_manager.h"

#include "compiled/warning_off.h"
#include <RmlUi/Core.h>
#include <algorithm>
#include "compiled/warning_on.h"

namespace evo
{
namespace
{
Rml::DataModelHandle model;

// The menus were laid out for a window of about 1000x700; smaller windows (phones) scale the
// whole UI down so it still fits.
Float UiScale(const IVec2 &size)
{
    const Float fit = std::min(static_cast<Float>(size.x) / 1000.0f, static_cast<Float>(size.y) / 700.0f);
    return std::clamp(fit, 0.5f, 1.0f);
}
} // namespace

Bool EvoUi::Initialize(const std::string &ui_dir)
{
    auto *manager = ui::Manager::GetInstance();
    if (!manager || !manager->GetContext())
    {
        LOGERROR("Evo Tournament needs the engine UI");
        return false;
    }
    if (!manager->LoadFontFace(ui_dir + "/../fonts/DejaVuSans-Bold.ttf", true))
    {
        return false;
    }

    Rml::DataModelConstructor c = manager->GetContext()->CreateDataModel("evo");
    if (!c)
    {
        return false;
    }
    if (auto s = c.RegisterStruct<UiFeedEntry>())
    {
        s.RegisterMember("killer", &UiFeedEntry::killer);
        s.RegisterMember("killerColor", &UiFeedEntry::killerColor);
        s.RegisterMember("weapon", &UiFeedEntry::weapon);
        s.RegisterMember("victim", &UiFeedEntry::victim);
        s.RegisterMember("victimColor", &UiFeedEntry::victimColor);
        s.RegisterMember("alpha", &UiFeedEntry::alpha);
        s.RegisterMember("involved", &UiFeedEntry::involved);
        s.RegisterMember("suicide", &UiFeedEntry::suicide);
    }
    if (auto s = c.RegisterStruct<UiNameplate>())
    {
        s.RegisterMember("name", &UiNameplate::name);
        s.RegisterMember("color", &UiNameplate::color);
        s.RegisterMember("x", &UiNameplate::x);
        s.RegisterMember("y", &UiNameplate::y);
        s.RegisterMember("size", &UiNameplate::size);
        s.RegisterMember("alpha", &UiNameplate::alpha);
    }
    if (auto s = c.RegisterStruct<UiWeaponSlot>())
    {
        s.RegisterMember("label", &UiWeaponSlot::label);
        s.RegisterMember("ammo", &UiWeaponSlot::ammo);
        s.RegisterMember("color", &UiWeaponSlot::color);
        s.RegisterMember("owned", &UiWeaponSlot::owned);
        s.RegisterMember("selected", &UiWeaponSlot::selected);
    }
    if (auto s = c.RegisterStruct<UiNotice>())
    {
        s.RegisterMember("text", &UiNotice::text);
        s.RegisterMember("alpha", &UiNotice::alpha);
    }
    if (auto s = c.RegisterStruct<UiScoreRow>())
    {
        s.RegisterMember("rank", &UiScoreRow::rank);
        s.RegisterMember("name", &UiScoreRow::name);
        s.RegisterMember("color", &UiScoreRow::color);
        s.RegisterMember("tag", &UiScoreRow::tag);
        s.RegisterMember("frags", &UiScoreRow::frags);
        s.RegisterMember("deaths", &UiScoreRow::deaths);
        s.RegisterMember("local", &UiScoreRow::local);
    }
    c.RegisterArray<std::vector<UiFeedEntry>>();
    c.RegisterArray<std::vector<UiNameplate>>();
    c.RegisterArray<std::vector<UiWeaponSlot>>();
    c.RegisterArray<std::vector<UiNotice>>();
    c.RegisterArray<std::vector<UiScoreRow>>();

    const auto form = [&](const Char *name, auto *pointer)
    {
        c.Bind(name, pointer);
        formVariables.emplace_back(name);
    };
    const auto display = [&](const Char *name, auto *pointer)
    {
        c.Bind(name, pointer);
        displayVariables.emplace_back(name);
    };
    UiState &s = state;
    form("name", &s.name);
    form("botCount", &s.botCount);
    form("botSkill", &s.botSkill);
    form("fragLimit", &s.fragLimit);
    form("timeLimit", &s.timeLimit);
    form("sensitivity", &s.sensitivity);
    form("fov", &s.fov);
    form("volume", &s.volume);
    form("invertY", &s.invertY);
    form("hostPort", &s.hostPort);
    form("joinText", &s.joinText);
    form("showMatch", &s.showMatch);
    form("showOptions", &s.showOptions);
    form("showControls", &s.showControls);
    form("showPauseOptions", &s.showPauseOptions);

    display("screen", &s.screen);
    display("skillName", &s.skillName);
    display("roomCodes", &s.roomCodes);
    display("onlineHelp", &s.onlineHelp);
    display("connectStatus", &s.connectStatus);
    display("error", &s.error);
    display("damage", &s.damage);
    display("alive", &s.alive);
    display("crosshair", &s.crosshair);
    display("hit", &s.hit);
    display("health", &s.health);
    display("armor", &s.armor);
    display("healthLow", &s.healthLow);
    display("healthAlpha", &s.healthAlpha);
    display("weaponName", &s.weaponName);
    display("weaponColor", &s.weaponColor);
    display("ammo", &s.ammo);
    display("slots", &s.slots);
    display("respawn", &s.respawn);
    display("respawnAlpha", &s.respawnAlpha);
    display("timer", &s.timer);
    display("timerLow", &s.timerLow);
    display("matchFragLimit", &s.matchFragLimit);
    display("hasRank", &s.hasRank);
    display("rank", &s.rank);
    display("rankFirst", &s.rankFirst);
    display("rankOf", &s.rankOf);
    display("fragsText", &s.fragsText);
    display("lead", &s.lead);
    display("leadGood", &s.leadGood);
    display("feed", &s.feed);
    display("nameplates", &s.nameplates);
    display("status", &s.status);
    display("notices", &s.notices);
    display("banner", &s.banner);
    display("bannerSub", &s.bannerSub);
    display("bannerColor", &s.bannerColor);
    display("bannerAlpha", &s.bannerAlpha);
    display("pickup", &s.pickup);
    display("pickupColor", &s.pickupColor);
    display("pickupAlpha", &s.pickupAlpha);
    display("scoreboard", &s.scoreboard);
    display("scoreTitle", &s.scoreTitle);
    display("scores", &s.scores);
    display("pause", &s.pause);
    display("pauseTitle", &s.pauseTitle);
    display("isHost", &s.isHost);
    display("roomCodeText", &s.roomCodeText);
    display("inviteLink", &s.inviteLink);
    display("friends", &s.friends);
    display("connectedTo", &s.connectedTo);
    display("showBots", &s.showBots);
    display("showResume", &s.showResume);

    c.BindEventCallback(
        "action",
        [this](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &arguments)
        {
            if (arguments.empty())
            {
                return;
            }
            const auto found = actions.find(arguments[0].Get<Rml::String>());
            if (found != actions.end() && found->second)
            {
                found->second();
            }
        }
    );
    model = c.GetModelHandle();

    document = manager->LoadDocument(ui_dir + "/evo.rml");
    if (!document)
    {
        return false;
    }
    document->Show();
    ready = true;
    Commit(true);
    return true;
}

void EvoUi::Shutdown()
{
    if (document)
    {
        document->Close();
        document = nullptr;
    }
    model = {};
    actions.clear();
    ready = false;
}

void EvoUi::OnAction(const std::string &name, std::function<void()> callback)
{
    actions[name] = std::move(callback);
}

void EvoUi::Commit(const Bool forms_changed)
{
    if (!ready)
    {
        return;
    }
    const IVec2 size = window::Manager::GetInstance()->GetWindowSize(0);
    if (size.x > 0 && size.y > 0)
    {
        scale = UiScale(size);
        ui::Manager::GetInstance()->SetDensityIndependentPixelRatio(scale);
    }
    for (const std::string &name : displayVariables)
    {
        model.DirtyVariable(name);
    }
    if (forms_changed)
    {
        for (const std::string &name : formVariables)
        {
            model.DirtyVariable(name);
        }
    }
}
} // namespace evo
