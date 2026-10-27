#pragma once
#include "stripe_canvas.h"
#include "passport_language.h"
namespace passport_wifi {
enum class Phase { Setup, Connecting, Pairing, Blocked, Online };
struct Frame {
    Phase phase = Phase::Connecting;
    char code[8]{};
    Language language = Language::Chinese;
    TextId link_title = TextId::Connecting, link_hint = TextId::ConnectingHint;
    TextId setup_title = TextId::SetupTitle;
    char title[80]{};
    char detail[384]{}, footer[80]{}, answer[384]{};
    uint32_t expires = 0, revision = 1;
    bool navigation = false;
};
void DrawScreen(raster::Canvas& canvas, const Frame& frame, int battery, const char* hotspot);
}
