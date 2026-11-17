#pragma once

// [Tablet]: the player's own menus held in the off hand (vr/TabletLogic.h,
// vr::TabletSettings). Read as one step of the key table (core/Config.cpp),
// its keys named there for the INI audit.

namespace obvr {

struct Config;

void ReadTabletStep(Config& config, const char* path);

}  // namespace obvr
