#pragma once
#include "ecs/core/game_application.h"

class EvoTournament final : public GameApplication
{
  public:
    Bool DerrivedInitialize() override;
    Bool UpdateLoop(const Double elapsed_seconds) override;
};
