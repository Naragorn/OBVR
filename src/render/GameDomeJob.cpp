#include "render/GameDomeJob.h"

#include <atomic>
#include <cstdio>
#include <string>

#include "core/Log.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "stage/DomeAtlas.h"
#include "stage/DomeBuild.h"
#include "stage/GameDomeKey.h"

namespace obvr::render {

namespace {

constexpr const char* kFolder = "OBVR_Stage\\sky";
constexpr const char* kName = "loading_dome_sky";
// zlib's level 3: the atlas 14.8 MB in 2.5 s; level 6 saves 0.8 MB for 2.7 s more (2026-11-15).
constexpr int kPackingLevel = 3;

enum : int { kIdle, kMaking, kReady, kFailed };

struct Job {
	std::atomic<int> state{kIdle};
	bool said = false;      // the outcome is in the log
	std::string dataDir;    // the game's Data
	std::string folder;     // OBVR_Stage\sky beside OBVR.dll
	std::string key;        // of the textures as they are now
	std::string why;        // the worker's reason, read after it is done
	double seconds = 0.0;
};

Job& TheJob() {
	static Job s_job;
	return s_job;
}

std::string Beside(const Job& job, const char* ending) { return job.folder + "\\" + kName + ending; }

bool FileThere(const std::string& path) { return GetFileAttributesA(path.c_str()) != 0xFFFFFFFFu; }

std::string ReadKeptKey(const std::string& path) {
	std::string text;
	if (std::FILE* file = std::fopen(path.c_str(), "rb")) {
		char buffer[1024];
		for (std::size_t got = 0; (got = std::fread(buffer, 1, sizeof(buffer), file)) > 0;) {
			text.append(buffer, got);
		}
		std::fclose(file);
	}
	return text;
}

// The key of the textures as they are: each one's own file under Data when
// there is one, else the archive it comes from.
std::string KeyNow(const std::string& dataDir) {
	UInt64 archiveTime = 0;
	UInt64 archiveSize = 0;
	const bool archive = ReadFileStamp(stage::JoinDataPath(dataDir.c_str(), stage::kGameTextureArchive).c_str(),
	                                   archiveTime, archiveSize);
	stage::TextureStamp stamps[stage::kGameTextureCount];
	for (int i = 0; i < stage::kGameTextureCount; ++i) {
		stage::TextureStamp& stamp = stamps[i];
		stamp.path = stage::kGameTexturePaths[i];
		stamp.loose = ReadFileStamp(stage::JoinDataPath(dataDir.c_str(), stamp.path).c_str(), stamp.time, stamp.size);
		stamp.found = stamp.loose || archive;
		if (!stamp.loose) {
			stamp.time = archiveTime;
			stamp.size = archiveSize;
		}
	}
	return stage::GameDomeKey(stage::kAtlasWidth, stamps, stage::kGameTextureCount);
}

// The worker: reads the textures, writes the dome, then its key - the key
// last, so a making cut short is made again.
DWORD __stdcall Make(void*) {
	Job& job = TheJob();
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	const long long start = ReadPerformanceCounter();
	bool ok = false;
	{
		stage::GameTextures textures;
		ok = stage::LoadGameTextures(job.dataDir.c_str(), textures, job.why) &&
		     stage::WriteDome(stage::DomeLook::Sky, stage::kAtlasWidth, &textures, job.folder.c_str(), kName,
		                      kPackingLevel, job.why);
	}
	if (ok) {
		std::FILE* file = std::fopen(Beside(job, ".key").c_str(), "wb");
		ok = file != nullptr && std::fwrite(job.key.data(), 1, job.key.size(), file) == job.key.size();
		if (file != nullptr) {
			std::fclose(file);
		}
		if (!ok) {
			job.why = "its key could not be written";
		}
	}
	const long long frequency = ReadPerformanceFrequency();
	job.seconds = frequency > 0 ? static_cast<double>(ReadPerformanceCounter() - start) / static_cast<double>(frequency) : 0.0;
	job.state.store(ok ? kReady : kFailed);
	return 0;
}

// The first call: where things are, whether the kept dome fits, the worker
// started when it does not.
void Begin(Job& job) {
	char path[512]{};
	if (!platform::BuildGamePath("Data", path, sizeof(path))) {
		job.why = "the game's folder is not known";
		job.state.store(kFailed);
		return;
	}
	job.dataDir = path;
	if (!platform::BuildPluginPath(kFolder, path, sizeof(path))) {
		job.why = "OBVR.dll's folder is not known";
		job.state.store(kFailed);
		return;
	}
	job.folder = path;
	job.key = KeyNow(job.dataDir);
	const bool there = FileThere(Beside(job, ".obj")) && FileThere(Beside(job, ".mtl")) && FileThere(Beside(job, ".png"));
	switch (stage::PlanGameDome(job.key, ReadKeptKey(Beside(job, ".key")), there)) {
		case stage::GameDomePlan::Use:
			job.said = true;
			job.state.store(kReady);
			OBVR_LOG("Render: the sky dome kept in %s fits the game's textures - used as it is", job.folder.c_str());
			return;
		case stage::GameDomePlan::GiveUp:
			job.why = std::string("a sky texture is neither under Data nor is there ") + stage::kGameTextureArchive;
			job.state.store(kFailed);
			return;
		case stage::GameDomePlan::Make:
			break;
	}
	CreateDirectoryA(job.folder.c_str(), nullptr);  // there already, or made; a failure shows when the atlas is written
	DeleteFileA(Beside(job, ".key").c_str());        // the kept dome no longer counts while it is written over
	job.state.store(kMaking);
	OBVR_LOG("Render: the sky dome is made from the game's textures into %s - the dark dome until it is there",
	         job.folder.c_str());
	HANDLE thread = CreateThread(nullptr, 0, &Make, nullptr, 0, nullptr);
	if (thread != nullptr) {
		CloseHandle(thread);
	} else {
		job.why = "no thread to make it on";
		job.state.store(kFailed);
	}
}

}  // namespace

bool GameDomeReady(char* modelPath, UInt32 modelPathSize) {
	Job& job = TheJob();
	int state = job.state.load();
	if (state == kIdle) {
		Begin(job);
		state = job.state.load();
	}
	if (state == kMaking) {
		return false;
	}
	if (!job.said) {
		job.said = true;
		if (state == kReady) {
			OBVR_LOG("Render: the sky dome is made (%.1f s)", job.seconds);
		} else {
			OBVR_LOG("Render: the sky dome could not be made (%s) - the dark dome stays", job.why.c_str());
		}
	}
	if (state != kReady) {
		return false;
	}
	const std::string model = Beside(job, ".obj");
	if (model.size() + 1 > modelPathSize) {
		return false;
	}
	std::snprintf(modelPath, modelPathSize, "%s", model.c_str());
	return true;
}

}  // namespace obvr::render
