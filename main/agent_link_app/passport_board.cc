#include "ai_passport_hardware.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
extern "C" {
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "bsp_pins.h"
}

namespace {
constexpr unsigned kRows = 4;
bool ColorDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx) {
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(ctx), &wake);
    return wake == pdTRUE;
}
}

esp_err_t PassportPanel::Init() {
    esp_err_t result = bsp_display_init();
    if (result != ESP_OK) return result;
    pixels_ = static_cast<uint8_t*>(heap_caps_malloc(BSP_LCD_W * kRows * 2, MALLOC_CAP_DMA));
    done_ = xSemaphoreCreateBinary();
    if (!Ready()) return ESP_ERR_NO_MEM;
    const esp_lcd_panel_io_callbacks_t cb = {.on_color_trans_done = ColorDone};
    result = esp_lcd_panel_io_register_event_callbacks(bsp_display_io(), &cb, done_);
    if (result == ESP_OK) bsp_display_backlight(70);
    return result;
}

esp_err_t PassportPanel::FlushStripe(uint16_t y, uint16_t rows) {
    if (!Ready() || rows > kRows || y + rows > BSP_LCD_H) return ESP_ERR_INVALID_ARG;
    // A failed transfer never permits buffer reuse; the renderer stops on failure.
    esp_err_t result = esp_lcd_panel_draw_bitmap(bsp_display_panel(), 0, y, BSP_LCD_W, y + rows, pixels_);
    if (result != ESP_OK) return result;
    return xSemaphoreTake(done_, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t PassportCodec::Init() {
    esp_err_t result = bsp_audio_init();
    if (result == ESP_OK) result = bsp_audio_set_format(16000, 16, 1);
    if (result == ESP_OK) bsp_audio_set_volume(0);
    return result;
}
esp_err_t PassportCodec::SetVolume(int percent) {
    std::lock_guard<std::mutex> guard(volume_lock_);
    volume_ = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    if (!muted_) bsp_audio_set_volume(volume_);
    return ESP_OK;
}
esp_err_t PassportCodec::SetMute(bool mute) {
    std::lock_guard<std::mutex> guard(volume_lock_);
    muted_ = mute;
    bsp_audio_set_volume(mute ? 0 : volume_);
    return ESP_OK;
}
esp_err_t PassportCodec::WritePcm(const int16_t* pcm, size_t samples) {
    return bsp_audio_write(pcm, samples * sizeof(int16_t));
}
esp_err_t PassportCodec::ReadPcm(int16_t* pcm, size_t capacity, size_t* got) {
    *got = 0;
    esp_err_t result = bsp_audio_read(pcm, capacity * sizeof(int16_t));
    if (result == ESP_OK) *got = capacity;
    return result;
}
void AiPassportHardware::Init() {
    display_ready = lcd.Init() == ESP_OK;
    (void)bsp_battery_init();
    audio_ready = codec.Init() == ESP_OK;
    if (!audio_ready) ESP_LOGW("Passport", "Audio unavailable; setup remains available");
}
