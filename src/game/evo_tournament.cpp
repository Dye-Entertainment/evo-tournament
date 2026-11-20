#include "game/evo_tournament.h"
#include "core/scene_manager.h"
#include "game/game_scene.h"

Bool EvoTournament::DerrivedInitialize()
{
    // SceneManager does not take ownership; the scene lives for the whole process.
    auto *scene = new evo::GameScene();
    core::SceneManager::GetInstance()->AddScene("Arena", scene);
    return true;
}

Bool EvoTournament::UpdateLoop(const Double elapsed_seconds)
{
    static_cast<void>(elapsed_seconds);
    return true;
}
