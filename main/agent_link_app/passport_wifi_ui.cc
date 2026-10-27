#include "passport_wifi_ui.h"
#include "passport_screen.h"
#include <cstdio>
#include <cstring>
#include <mutex>
#include "esp_mac.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
extern "C" {
#include "bsp_battery.h"
#include "bsp_display.h"
}

namespace passport_wifi {
namespace {
Frame input, frame;
std::mutex lock;
PassportPanel* panel = nullptr;
char hotspot[33]{};
void Render(void*) {
    uint32_t seen = 0;
    unsigned tick = 0;
    int battery = -1;
    for (;;) {
        if (++tick >= 100) {
            battery = bsp_battery_soc(); tick = 0;
            std::lock_guard<std::mutex> guard(lock);
            ++input.revision;
        }
        {
            std::lock_guard<std::mutex> guard(lock);
            if (seen != input.revision) frame = input;
        }
        if (seen == frame.revision) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        for (int y = 0; y < 320; y += 4) {
            raster::Canvas canvas(panel->StripeBuffer(), 240, y, 4);
            DrawScreen(canvas, frame, battery, hotspot);
            if (panel->FlushStripe(y, 4) != ESP_OK) {
                ESP_LOGE("Passport.ui", "Display transfer failed; renderer stopped to protect DMA buffer");
                vTaskDelete(nullptr);
            }
            if (y % 32 == 28) vTaskDelay(1);
        }
        seen = frame.revision;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
}
esp_err_t StartUi(PassportPanel* lcd) {
    if (!lcd || !lcd->Ready()) return ESP_ERR_INVALID_STATE;
    panel = lcd;
    input.language = GetLanguage();
    uint8_t mac[6]{};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(hotspot, sizeof hotspot, "DeotaPassport-%02X%02X", mac[4], mac[5]);
    return xTaskCreate(Render, "passport_ui", 4096, nullptr, 2, nullptr) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
void SetStatus(const agent_link_status_t& status) {
    std::lock_guard<std::mutex> guard(lock);
    input.phase = status.phase == AGENT_LINK_PHASE_SETUP ? Phase::Setup :
        status.phase == AGENT_LINK_PHASE_PAIRING ? Phase::Pairing :
        status.phase == AGENT_LINK_PHASE_BLOCKED ? Phase::Blocked :
        status.phase == AGENT_LINK_PHASE_CONNECTED || status.phase == AGENT_LINK_PHASE_READY ? Phase::Online : Phase::Connecting;
    snprintf(input.code, sizeof input.code, "%s", status.code);
    input.link_title = TextId::Connecting; input.link_hint = TextId::ConnectingHint;
    if (strcmp(status.title, "Joining WiFi") == 0) { input.link_title = TextId::Joining; input.link_hint = TextId::JoiningHint; }
    else if (strcmp(status.title, "Signing in") == 0) { input.link_title = TextId::SigningIn; input.link_hint = TextId::SigningInHint; }
    else if (strcmp(status.title, "Reconnecting") == 0) { input.link_title = TextId::Retrying; input.link_hint = TextId::RetryingHint; }
    else if (strcmp(status.title, "Already bound") == 0) { input.link_title = TextId::AlreadyBound; input.link_hint = TextId::AlreadyBoundHint; }
    else if (strcmp(status.title, "No agent") == 0) { input.link_title = TextId::NoAgent; input.link_hint = TextId::NoAgentHint; }
    else if (input.phase == Phase::Blocked) { input.link_title = TextId::PlatformError; input.link_hint = TextId::PlatformErrorHint; }
    input.setup_title = TextId::SetupTitle;
    if (strstr(status.hint, "Authentication failed")) input.setup_title = TextId::SetupBadPassword;
    else if (strstr(status.hint, "Network not found")) input.setup_title = TextId::SetupNotFound;
    else if (strstr(status.hint, "timed out") || strstr(status.hint, "No IP")) input.setup_title = TextId::SetupFailed;
    input.expires = status.expires_s;
    ++input.revision;
}
void SetContent(const char* title, const char* detail, const char* footer, bool navigation) {
    std::lock_guard<std::mutex> guard(lock);
    if (strcmp(input.title, title) == 0 && strcmp(input.detail, detail) == 0 &&
        strcmp(input.footer, footer) == 0 && input.navigation == navigation && input.language == GetLanguage()) return;
    raster::CopyText(input.title, sizeof input.title, title);
    raster::CopyText(input.detail, sizeof input.detail, detail);
    raster::CopyText(input.footer, sizeof input.footer, footer);
    input.navigation = navigation;
    input.language = GetLanguage();
    ++input.revision;
}
void SetAnswer(const char* text) {
    if (!text) return;
    std::lock_guard<std::mutex> guard(lock);
    raster::CopyText(input.answer, sizeof input.answer, text);
    ++input.revision;
}
}  // namespace passport_wifi
