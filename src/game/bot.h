#pragma once
#include "game/world.h"

namespace evo
{
void UpdateBot(World &world, int player_id, Float delta_seconds, MoveInput &input, FireRequest &fire);

const Char *BotName(int index);
} // namespace evo
