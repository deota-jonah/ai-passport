#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cstddef>
#include <cstdint>
#include <mutex>

// Application adapters use only the official BSP. No pin or codec register copies.
class PassportPanel {
public:
    esp_err_t Init();
    bool Ready() const { return pixels_ && done_; }
    uint8_t* StripeBuffer() const { return pixels_; }
    esp_err_t FlushStripe(uint16_t y, uint16_t rows);
private:
    uint8_t* pixels_ = nullptr;
    SemaphoreHandle_t done_ = nullptr;
};

class PassportCodec {
public:
    esp_err_t Init();
    esp_err_t SetVolume(int percent);
    esp_err_t SetMute(bool mute);
    esp_err_t WritePcm(const int16_t* pcm, size_t samples);
    esp_err_t ReadPcm(int16_t* pcm, size_t capacity, size_t* got);
    bool MicOn() const { return mic_; }
    esp_err_t StartMic() { mic_ = true; return ESP_OK; }
    esp_err_t StopMic() { mic_ = false; return ESP_OK; }
private:
    std::mutex volume_lock_;
    int volume_ = 70;
    bool mic_ = false, muted_ = true;
};

class AiPassportHardware {
public:
    void Init();
    PassportPanel lcd;
    PassportCodec codec;
    bool display_ready = false, audio_ready = false, keys_ready = false;
};
