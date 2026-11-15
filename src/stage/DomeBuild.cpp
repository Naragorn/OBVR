#include "stage/DomeBuild.h"

#include <cstdio>
#include <vector>

#include "miniz.h"
#include "stage/BsaArchive.h"
#include "stage/DdsImage.h"
#include "stage/DomeAtlas.h"
#include "stage/PngStream.h"

namespace obvr::stage {

namespace {

#if defined(_WIN32)
constexpr char kSeparator = '\\';
#else
constexpr char kSeparator = '/';
#endif

constexpr std::size_t kLargestTextureBytes = 96u * 1024u * 1024u;  // an 8192 x 4096 picture unpacked is 128 MB; no sky file is near

struct File {
	std::FILE* handle = nullptr;
	explicit File(const std::string& path, const char* mode) : handle(std::fopen(path.c_str(), mode)) {}
	~File() {
		if (handle != nullptr) {
			std::fclose(handle);
		}
	}
	File(const File&) = delete;
	File& operator=(const File&) = delete;
};

bool ReadAt(std::FILE* file, std::uint32_t offset, std::uint8_t* out, std::size_t count) {
	// fseek's long is 32 bits on Windows: an archive past 2 GB is not read (the game's is 1.2 GB).
	return offset <= 0x7FFFFFFFu && std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0 &&
	       std::fread(out, 1, count, file) == count;
}

bool ReadWholeFile(const std::string& path, std::vector<std::uint8_t>& out) {
	File file(path, "rb");
	if (file.handle == nullptr || std::fseek(file.handle, 0, SEEK_END) != 0) {
		return false;
	}
	const long size = std::ftell(file.handle);
	if (size <= 0 || static_cast<std::size_t>(size) > kLargestTextureBytes) {
		return false;
	}
	out.resize(static_cast<std::size_t>(size));
	return ReadAt(file.handle, 0, out.data(), out.size());
}

// The game's archive, its directory read once for all six lookups.
struct Archive {
	File file;
	BsaHeader header;
	std::vector<std::uint8_t> directory;

	explicit Archive(const std::string& path) : file(path, "rb") {
		std::uint8_t head[kBsaHeaderBytes];
		if (file.handle == nullptr || !ReadAt(file.handle, 0, head, sizeof(head))) {
			return;
		}
		header = ReadBsaHeader(head, sizeof(head));
		const std::uint64_t bytes = header.valid ? BsaDirectoryBytes(header) : 0;
		if (bytes == 0 || bytes > kLargestTextureBytes) {
			header.valid = false;
			return;
		}
		directory.resize(static_cast<std::size_t>(bytes));
		header.valid = ReadAt(file.handle, 0, directory.data(), directory.size());
	}

	// The file's bytes, unpacked.
	bool Read(const char* path, std::vector<std::uint8_t>& out, std::string& why) {
		BsaEntry entry;
		if (!header.valid || !FindInBsa(directory.data(), directory.size(), header, path, entry)) {
			why = std::string(path) + " is neither a file under Data nor in " + kGameTextureArchive;
			return false;
		}
		std::vector<std::uint8_t> stored(entry.size);
		if (entry.size < 4 || entry.size > kLargestTextureBytes || !ReadAt(file.handle, entry.offset, stored.data(), stored.size())) {
			why = std::string(path) + " could not be read from " + kGameTextureArchive;
			return false;
		}
		if (!entry.compressed) {
			out.swap(stored);
			return true;
		}
		const std::uint32_t full = ReadU32(stored.data());
		if (full == 0 || full > kLargestTextureBytes) {
			why = std::string(path) + " has no sane size in " + kGameTextureArchive;
			return false;
		}
		out.resize(full);
		mz_ulong unpacked = full;
		if (mz_uncompress(out.data(), &unpacked, stored.data() + 4, static_cast<mz_ulong>(stored.size() - 4)) != MZ_OK ||
		    unpacked != full) {
			why = std::string(path) + " could not be unpacked from " + kGameTextureArchive;
			return false;
		}
		return true;
	}
};

bool WriteText(const std::string& path, const std::string& text) {
	File file(path, "wb");
	return file.handle != nullptr && std::fwrite(text.data(), 1, text.size(), file.handle) == text.size();
}

DomeScene SceneFor(DomeLook look, const AtlasLayout& layout, const GameTextures* textures) {
	DomeScene scene;
	scene.look = look;
	if (look == DomeLook::Dark) {
		scene.octaves = 7;
		scene.stars = MakeStarField(layout);
		return scene;
	}
	scene.octaves = 5;
	if (textures != nullptr) {
		scene.sky.stars = &textures->images[0];
		scene.sky.stars01 = &textures->images[1];
		scene.sky.nebula = &textures->images[2];
		scene.moons[0] = PlaceMoon(&textures->images[3], 35.0, 24.0, 18.0);  // Masser, to the right of ahead
		scene.moons[1] = PlaceMoon(&textures->images[4], 62.0, 31.0, 7.0);   // Secunda
		scene.map = &textures->images[5];
	}
	return scene;
}

bool WriteAtlas(const DomeScene& scene, const AtlasLayout& layout, const std::string& path, int level) {
	File file(path, "wb");
	if (file.handle == nullptr) {
		return false;
	}
	std::FILE* handle = file.handle;
	PngStream png;
	if (!png.Open([handle](const void* bytes, std::size_t count) { return std::fwrite(bytes, 1, count, handle) == count; },
	              layout.width, layout.height, level)) {
		return false;
	}
	std::vector<double> work(static_cast<std::size_t>(layout.width) * 3);
	std::vector<std::uint8_t> row(static_cast<std::size_t>(layout.width) * 3);
	for (int y = 0; y < layout.height; ++y) {
		PaintAtlasRow(scene, layout, y, work.data(), row.data());
		if (!png.WriteRow(row.data())) {
			return false;
		}
	}
	return png.Close() && std::fflush(handle) == 0;
}

}  // namespace

std::string JoinDataPath(const char* dataDir, const char* path) {
	std::string joined = std::string(dataDir) + kSeparator + path;
	for (char& c : joined) {
		if (c == '\\' || c == '/') {
			c = kSeparator;
		}
	}
	return joined;
}

bool GameTextureIsLoose(const char* dataDir, const char* path) {
	File file(JoinDataPath(dataDir, path), "rb");
	return file.handle != nullptr;
}

bool LoadGameTextures(const char* dataDir, GameTextures& out, std::string& why) {
	Archive archive(JoinDataPath(dataDir, kGameTextureArchive));
	for (int i = 0; i < kGameTextureCount; ++i) {
		const char* path = kGameTexturePaths[i];
		std::vector<std::uint8_t> bytes;
		if (GameTextureIsLoose(dataDir, path)) {
			if (!ReadWholeFile(JoinDataPath(dataDir, path), bytes)) {
				why = std::string(path) + " under Data could not be read";
				return false;
			}
		} else if (!archive.Read(path, bytes, why)) {
			return false;
		}
		if (!DecodeDds(bytes.data(), bytes.size(), out.images[i])) {
			why = std::string(path) + " is not a DDS this reads (DXT1, DXT3, DXT5 or plain 24/32 bits)";
			return false;
		}
	}
	return true;
}

bool WriteDome(DomeLook look, int width, const GameTextures* textures, const char* directory, const char* name,
               int level, std::string& why) {
	if (width < 64 || width % 16 != 0) {
		why = "the atlas's width is not a multiple of 16";
		return false;
	}
	const AtlasLayout layout = LayoutFor(width);
	const DomeScene scene = SceneFor(look, layout, textures);
	if (!SceneComplete(scene)) {
		why = "the sky look has not all of its textures";
		return false;
	}
	const std::string base = JoinDataPath(directory, name);
	const std::string atlas = base + ".png";
	const std::string unfinished = atlas + ".part";
	if (!WriteAtlas(scene, layout, unfinished, level)) {
		std::remove(unfinished.c_str());
		why = "the atlas could not be written to " + unfinished;
		return false;
	}
	std::remove(atlas.c_str());  // rename does not replace on Windows
	if (std::rename(unfinished.c_str(), atlas.c_str()) != 0) {
		why = "the atlas could not be moved to " + atlas;
		return false;
	}
	if (!WriteText(base + ".mtl", DomeMtlText(name)) || !WriteText(base + ".obj", DomeObjText(layout, name))) {
		why = "the model could not be written to " + base + ".obj";
		return false;
	}
	return true;
}

}  // namespace obvr::stage
