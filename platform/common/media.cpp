#include "media.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "stb_image.h"
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

namespace host {

bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool LoadImageRgba(const std::string& path, Image& out, bool premultiply) {
    std::vector<uint8_t> file;
    if (!ReadBinaryFile(path, file)) return false;
    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(file.data(), static_cast<int>(file.size()), &w, &h, &channels, 4);
    if (!pixels) return false;
    out.width = w;
    out.height = h;
    out.rgba.assign(pixels, pixels + static_cast<size_t>(w) * h * 4);
    stbi_image_free(pixels);
    if (premultiply) {
        for (size_t i = 0; i < out.rgba.size(); i += 4) {
            const unsigned a = out.rgba[i + 3];
            for (int c = 0; c < 3; c++) {
                unsigned prod = out.rgba[i + c] * a + 128;   // SkMulDiv255Round
                out.rgba[i + c] = static_cast<uint8_t>((prod + (prod >> 8)) >> 8);
            }
        }
    }
    return true;
}

bool DecodeOgg(const std::string& path, Pcm& out) {
    std::vector<uint8_t> file;
    if (!ReadBinaryFile(path, file)) return false;
    int channels = 0, rate = 0;
    short* samples = nullptr;
    int frames = stb_vorbis_decode_memory(file.data(), static_cast<int>(file.size()), &channels, &rate, &samples);
    if (frames < 0 || !samples) return false;
    out.channels = channels;
    out.sampleRate = rate;
    out.samples.assign(samples, samples + static_cast<size_t>(frames) * channels);
    std::free(samples);
    return true;
}

}  // namespace host
