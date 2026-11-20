#include "core/entry.h"
#include "core/command_line.h"
#include "game/evo_tournament.h"
#include "game/self_test.h"

#include "compiled/warning_off.h"
#include <cstdlib>
#include "compiled/warning_on.h"

namespace
{
Bool runSelfTest = false;
COMMAND_LINE_OPTION("-selftest", runSelfTest);
Bool runNetTest = false;
COMMAND_LINE_OPTION("-nettest", runNetTest);
} // namespace

int core::entry()
{
    if (runSelfTest)
    {
        std::exit(evo::RunSelfTest());
    }
#ifndef __EMSCRIPTEN__
    if (runNetTest)
    {
        std::exit(evo::RunNetTest());
    }
#endif
    EvoTournament application;
    core::WindowInfo window_info;
    window_info.windowName = "Evo Tournament";
    window_info.windowSize = {1280, 720};
    if (application.Initialize(window_info))
    {
        application.Update();
    }
    application.Destroy();
    return 0;
}
