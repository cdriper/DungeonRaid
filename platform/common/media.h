// Decoding the game's PNG textures and Ogg sounds (stb_image, stb_vorbis), shared by the hosts.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace host {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;   // width * height * 4, top row first
};

// Loads a PNG as RGBA. With `premultiply`, colour is multiplied by alpha the way Android's decoder
// does (Skia's rounding): the Android shell uploads premultiplied bitmaps, and the game's blending
// (SRC_ALPHA, ONE_MINUS_SRC_ALPHA) was tuned on that.
bool LoadImageRgba(const std::string& path, Image& out, bool premultiply);

struct Pcm {
    int channels = 0;
    int sampleRate = 0;
    std::vector<int16_t> samples;   // interleaved
};

bool DecodeOgg(const std::string& path, Pcm& out);

// Reads a whole file (UTF-8 path).
bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>& out);

}  // namespace host
