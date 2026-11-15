// Writes a loading stage (src/vr/LoadingStage.h) with the game's own painter (src/stage):
//
//   make_loading_stage dark <folder> [width] [level]
//       OBVR's own dark dome, the one that ships: loading_dome_dark.obj, .mtl and .png.
//       For the repository: make_loading_stage dark assets/stage
//
//   make_loading_stage sky <Data folder> <folder> [width] [level]
//       The sky dome from a game's textures, as OBVR makes it in the player's game
//       (src/render/GameDomeJob.cpp) - to look at it, and to time its making. Not for
//       the repository: the game's textures are Bethesda's and are not shipped.
//
// width is the atlas's - 12288 for the dark dome, 8192 for the sky unless given; level its
// packing, 1 (fast) to 9 (small).
// Built with the tests (tests/CMakeLists.txt, target make_loading_stage).

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "stage/DomeAtlas.h"
#include "stage/DomeBuild.h"

int main(int argc, char** argv) {
	using namespace obvr::stage;
	const bool dark = argc >= 3 && std::strcmp(argv[1], "dark") == 0;
	const bool sky = argc >= 4 && std::strcmp(argv[1], "sky") == 0;
	if (!dark && !sky) {
		std::fprintf(stderr, "usage: make_loading_stage dark <folder> [width] [level]\n"
		                     "       make_loading_stage sky <Data folder> <folder> [width] [level]\n");
		return 2;
	}
	const int first = dark ? 3 : 4;
	const int width = argc > first ? std::atoi(argv[first]) : (dark ? kDarkAtlasWidth : kAtlasWidth);
	const int level = argc > first + 1 ? std::atoi(argv[first + 1]) : (dark ? 9 : 6);
	const char* folder = argv[first - 1];
	std::string why;
	const auto start = std::chrono::steady_clock::now();
	GameTextures textures;
	if (sky && !LoadGameTextures(argv[2], textures, why)) {
		std::fprintf(stderr, "the game's textures: %s\n", why.c_str());
		return 1;
	}
	const auto loaded = std::chrono::steady_clock::now();
	const char* name = dark ? "loading_dome_dark" : "loading_dome_sky";
	if (!WriteDome(dark ? DomeLook::Dark : DomeLook::Sky, width, sky ? &textures : nullptr, folder, name, level, why)) {
		std::fprintf(stderr, "the dome: %s\n", why.c_str());
		return 1;
	}
	const auto done = std::chrono::steady_clock::now();
	std::printf("Wrote %s/%s.obj, .mtl and .png (%d x %d, level %d): textures %.2f s, dome %.2f s\n", folder, name, width,
	            width / 2, level, std::chrono::duration<double>(loaded - start).count(),
	            std::chrono::duration<double>(done - loaded).count());
	return 0;
}
