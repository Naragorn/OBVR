#pragma once

// A dome written to disk (vr/LoadingStage.h): its model, its material and
// its atlas, painted by stage/DomeAtlas.h. The dark look needs nothing; the
// sky look reads the game's own textures from the player's Data folder - a
// texture's own file when there is one (a texture mod's), else the game's
// archive. Plain C files, no engine: it runs on a worker thread in the game
// (render/GameDomeJob.cpp) and in tools/make-loading-stage.cpp.

#include <string>

#include "stage/DomeImage.h"
#include "stage/DomeLook.h"

namespace obvr::stage {

inline constexpr const char* kGameTextureArchive = "Oblivion - Textures - Compressed.bsa";

inline constexpr int kGameTextureCount = 6;
// In GameTextures' order.
inline constexpr const char* kGameTexturePaths[kGameTextureCount] = {
	"textures\\sky\\skystars.dds",        "textures\\sky\\skystars01.dds",  "textures\\sky\\skynebula01.dds",
	"textures\\sky\\masser_full.dds",     "textures\\sky\\secunda_full.dds",
	"textures\\menus\\map\\world\\cyrodiil_resized.dds",
};

struct GameTextures {
	Image images[kGameTextureCount];
};

// "<dataDir><separator><path>", the path's slashes the platform's.
std::string JoinDataPath(const char* dataDir, const char* path);

// Whether the texture has its own file under Data.
bool GameTextureIsLoose(const char* dataDir, const char* path);

// All six, each from its own file or the archive. False with the reason in
// why when one cannot be read or decoded.
bool LoadGameTextures(const char* dataDir, GameTextures& out, std::string& why);

// Writes <directory>/<name>.obj, .mtl and .png. textures is the sky look's
// and may be null for the dark one. level is the atlas's packing, 1 (fast)
// to 9 (small). The atlas is written beside its place and moved there when
// it is whole, so a making cut short leaves no half picture under the name.
bool WriteDome(DomeLook look, int width, const GameTextures* textures, const char* directory, const char* name,
               int level, std::string& why);

}  // namespace obvr::stage
