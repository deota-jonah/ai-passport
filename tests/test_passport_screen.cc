#include "passport_screen.h"
#include "passport_wifi_controls.h"
#include "voice_playback.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <regex>

static void Screenshot(const char* directory, const char* name, const passport_wifi::Frame& frame) {
    const std::string path = std::string(directory) + "/" + name + ".ppm";
    std::ofstream out(path, std::ios::binary);
    out << "P6\n240 320\n255\n";
    uint8_t pixels[240 * 4 * 2]{};
    for (int y = 0; y < 320; y += 4) {
        raster::Canvas canvas(pixels, 240, y, 4);
        passport_wifi::DrawScreen(canvas, frame, 78, "DeotaPassport-AB12");
        if (y == 0 || y == 316) assert(pixels[0] == 0 && pixels[1] == 0); // Screen corner stays black.
        for (int i = 0; i < 240 * 4; ++i) {
            const uint16_t c = (pixels[2 * i] << 8) | pixels[2 * i + 1];
            const char rgb[] = {static_cast<char>(((c >> 11) & 31) * 255 / 31),
                                static_cast<char>(((c >> 5) & 63) * 255 / 63),
                                static_cast<char>((c & 31) * 255 / 31)};
            out.write(rgb, 3);
        }
    }
}
int main(int argc, char** argv) {
    assert(argc == 3);
    std::ifstream inventory(argv[1], std::ios::binary);
    const std::string required{std::istreambuf_iterator<char>(inventory), std::istreambuf_iterator<char>()};
    const char* at = required.data();
    const char* end = at + required.size();
    while (at < end) {
        const auto cp = raster::NextCodepoint(at, end);
        if (!raster::GlyphId(raster_cjk_14, cp)) {
            std::fprintf(stderr, "Missing glyph U+%04X\n", static_cast<unsigned>(cp));
            return 1;
        }
    }
    assert(!raster::GlyphId(raster_cjk_14, 0x1f984)); // Unsupported emoji remains detectable.
    char clipped[5];
    raster::CopyText(clipped, sizeof clipped, "设备码");
    assert(std::strcmp(clipped, "设") == 0);
    kc_audio::PlaybackGate gate(3200, 4096);
    assert(!gate.Ready(100, false, 0));
    assert(gate.Ready(3200, false, 1));
    assert(!gate.Ready(0, false, 2));
    assert(!gate.Ready(0, false, 90));
    assert(!gate.Ready(3200, false, 91));
    assert(gate.Ready(100, true, 92)); // A short final reply still drains.
    using namespace passport_wifi;
    assert(StoredLanguage(0) == Language::Chinese);
    assert(StoredLanguage(1) == Language::English);
    assert(StoredLanguage(255) == Language::Chinese);
    SetLanguage(Language::English); assert(GetLanguage() == Language::English);
    SetLanguage(Language::Chinese); assert(GetLanguage() == Language::Chinese);
    for (unsigned id = 0; id < static_cast<unsigned>(TextId::Count); ++id) {
        const auto text = static_cast<TextId>(id);
        const std::string en = Translate(text, Language::English);
        const std::string zh = std::regex_replace(Translate(text, Language::Chinese), std::regex("%[a-z%]+"), "");
        assert(!en.empty() && !zh.empty());
        for (unsigned char ch : en) assert(ch < 128); // English UI has no Chinese text.
        for (unsigned char ch : zh) assert(!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')));
    }
    for (const auto language : {Language::Chinese, Language::English}) {
        const std::string prefix = language == Language::Chinese ? "zh-" : "en-";
        auto tr = [&](TextId id) { return Translate(id, language); };
        Frame frame; frame.language = language;
        frame.phase = Phase::Setup;
        Screenshot(argv[2], (prefix + "setup").c_str(), frame);
        frame.phase = Phase::Pairing;
        std::strcpy(frame.code, "123456"); frame.expires = 598;
        Screenshot(argv[2], (prefix + "pairing").c_str(), frame);
        frame.expires = 0;
        Screenshot(argv[2], (prefix + "expired").c_str(), frame);
        frame.phase = Phase::Online;
        std::strcpy(frame.title, tr(TextId::HomeTitle));
        std::snprintf(frame.detail, sizeof frame.detail, "%s\n\n%s", tr(TextId::Selected), tr(TextId::ContinueLatest));
        std::strcpy(frame.footer, tr(TextId::HomeFooter));
        Screenshot(argv[2], (prefix + "ready").c_str(), frame);
        frame.navigation = true;
        std::strcpy(frame.title, tr(TextId::LanguageTitle));
        std::snprintf(frame.detail, sizeof frame.detail, "> %s\n\n  %s\n\n  %s", tr(TextId::ChineseOption), tr(TextId::EnglishOption), tr(TextId::Back));
        std::strcpy(frame.footer, tr(TextId::MenuFooter));
        Screenshot(argv[2], (prefix + "language").c_str(), frame);
    }
    std::puts("Passport screen, glyph coverage and playback tests: PASS");
}
