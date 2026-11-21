// Build-time tool: packs PNG files into one .ico (PNG-compressed entries, Windows Vista and later),
// so the exe's icon is made from the original's Android icons (assets/icons/*/icon.png) instead of
// a generated file kept in the repo.
//
// Usage: make_ico <out.ico> <in1.png> [in2.png ...]
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

void Put16(std::vector<uint8_t>& v, unsigned x) {
    v.push_back(static_cast<uint8_t>(x));
    v.push_back(static_cast<uint8_t>(x >> 8));
}

void Put32(std::vector<uint8_t>& v, uint32_t x) {
    Put16(v, x & 0xffff);
    Put16(v, x >> 16);
}

uint32_t Be32(const std::vector<uint8_t>& b, size_t at) {
    return (uint32_t{b[at]} << 24) | (uint32_t{b[at + 1]} << 16) | (uint32_t{b[at + 2]} << 8) | b[at + 3];
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: make_ico <out.ico> <in.png>...\n");
        return 2;
    }
    std::vector<std::vector<uint8_t>> pngs;
    for (int i = 2; i < argc; i++) {
        std::ifstream in(argv[i], std::ios::binary);
        std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        static const uint8_t kSig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
        if (b.size() < 24 || !std::equal(kSig, kSig + 8, b.begin())) {
            std::fprintf(stderr, "make_ico: %s is not a PNG\n", argv[i]);
            return 1;
        }
        pngs.push_back(std::move(b));
    }

    std::vector<uint8_t> ico;
    Put16(ico, 0);   // reserved
    Put16(ico, 1);   // type: icon
    Put16(ico, static_cast<unsigned>(pngs.size()));
    uint32_t offset = 6 + 16 * static_cast<uint32_t>(pngs.size());
    for (const auto& png : pngs) {
        const uint32_t w = Be32(png, 16), h = Be32(png, 20);   // IHDR
        ico.push_back(static_cast<uint8_t>(w >= 256 ? 0 : w));
        ico.push_back(static_cast<uint8_t>(h >= 256 ? 0 : h));
        ico.push_back(0);   // palette size
        ico.push_back(0);   // reserved
        Put16(ico, 1);      // planes
        Put16(ico, 32);     // bits per pixel
        Put32(ico, static_cast<uint32_t>(png.size()));
        Put32(ico, offset);
        offset += static_cast<uint32_t>(png.size());
    }
    for (const auto& png : pngs) ico.insert(ico.end(), png.begin(), png.end());

    std::ofstream out(argv[1], std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(ico.data()), static_cast<std::streamsize>(ico.size()));
    if (!out) {
        std::fprintf(stderr, "make_ico: cannot write %s\n", argv[1]);
        return 1;
    }
    return 0;
}
