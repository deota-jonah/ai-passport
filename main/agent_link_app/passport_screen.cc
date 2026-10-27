#include "passport_screen.h"
#include <cstdio>
#include <cstring>

namespace passport_wifi {
namespace {
void Text(raster::Canvas& canvas, const raster_font_t& font, const char* text,
          int x, int y, int width, unsigned lines, uint32_t color = 0xCFDBE3) {
    raster::TextLayout layout;
    layout.Wrap(font, text, width);
    for (unsigned i = 0; i < layout.Count() && i < lines; ++i)
        canvas.Text(font, x, y + i * (font.line_height + 3),
                    text + layout.Start(i), text + layout.End(i), color);
}
}
void DrawScreen(raster::Canvas& canvas, const Frame& frame, int battery, const char* hotspot) {
    auto tr = [&](TextId id) { return Translate(id, frame.language); };
    constexpr uint32_t accent = 0x64E1C4;
    canvas.Fill(0x000000);
    canvas.Box({0, 0, 240, 320}, 0x101D29, 30);
    canvas.Box({12, 44, 216, 225}, 0x203342, 14);
    Text(canvas, raster_cjk_14, tr(TextId::Header), 20, 14, 150, 1, accent);
    char battery_text[12];
    if (battery >= 0) snprintf(battery_text, sizeof battery_text, "%d%%", battery);
    else snprintf(battery_text, sizeof battery_text, "--");
    Text(canvas, raster_montserrat_14, battery_text, 184, 14, 44, 1);
    canvas.Clip({24, 54, 192, 205});
    if (!frame.navigation && frame.phase == Phase::Pairing) {
        Text(canvas, raster_cjk_14, tr(TextId::CodeTitle), 24, 56, 192, 1, accent);
        const bool valid = frame.expires > 0 && std::strlen(frame.code) == 6;
        Text(canvas, raster_montserrat_28, valid ? frame.code : "------", 24, 82, 192, 1, 0xFFFFFF);
        Text(canvas, raster_cjk_14, tr(TextId::CodeInstructions), 24, 124, 192, 2);
        Text(canvas, raster_montserrat_14, "bot.deotaland.cn\n/#/devices", 24, 177, 192, 2, accent);
        char remaining[64];
        snprintf(remaining, sizeof remaining, valid ? tr(TextId::CodeRemaining) : tr(TextId::CodeRefreshing), static_cast<unsigned long>(frame.expires));
        Text(canvas, raster_cjk_14, remaining, 24, 231, 192, 1);
    } else if (!frame.navigation && frame.phase == Phase::Setup) {
        Text(canvas, raster_cjk_14, tr(frame.setup_title), 24, 56, 192, 1, accent);
        Text(canvas, raster_cjk_14, tr(TextId::SetupPhone), 24, 90, 192, 1);
        Text(canvas, raster_montserrat_14, hotspot, 24, 117, 192, 2, 0xFFFFFF);
        Text(canvas, raster_cjk_14, tr(TextId::SetupBrowser), 24, 159, 192, 1);
        Text(canvas, raster_montserrat_20, "192.168.4.1", 24, 185, 192, 1, accent);
        Text(canvas, raster_cjk_14, tr(TextId::SetupCredentials), 24, 219, 192, 2);
    } else if (!frame.navigation && frame.phase != Phase::Online) {
        Text(canvas, raster_cjk_14, tr(frame.link_title), 24, 56, 192, 2, accent);
        Text(canvas, raster_cjk_14, tr(frame.link_hint), 24, 113, 192, 5);
        Text(canvas, raster_montserrat_14, "bot.deotaland.cn\n/#/devices", 24, 221, 192, 2, accent);
    } else {
        Text(canvas, raster_cjk_14, frame.title, 24, 56, 192, 2, accent);
        Text(canvas, raster_cjk_14, frame.detail, 24, 99, 192, 5);
        if (!frame.navigation) Text(canvas, raster_cjk_14, frame.answer, 24, 218, 192, 2, accent);
    }
    canvas.Clip({14, 277, 212, 38});
    Text(canvas, raster_cjk_14, frame.navigation ? frame.footer : frame.phase == Phase::Online ? frame.footer : tr(TextId::SetupFooter), 16, 279, 208, 2);
}
}
