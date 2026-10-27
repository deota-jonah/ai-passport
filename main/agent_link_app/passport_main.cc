#include "application.h"
#include "passport_config.h"
#include "passport_language.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Optional portal localization hook supplied to the vendored Wi-Fi component.
extern "C" bool al_wifi_prov_use_english(void) {
    return passport_wifi::GetLanguage() == passport_wifi::Language::English;
}

extern "C" void app_main(void) {
    Application& app = Application::GetInstance();
    static agent_output_cb_t output{};
    output.on_audio_out = [](const uint8_t* pcm, size_t bytes, void*) { Application::GetInstance().PlayAudio(pcm, bytes); };
    output.on_audio_end = [](void*) { Application::GetInstance().AudioEnd(); };
    output.on_audio_reset = [](void*) { Application::GetInstance().AudioReset(); };
    output.on_show_text = [](const char* text, void*) { Application::GetInstance().ShowText(text); };
    output.on_listen = [](bool start, uint32_t limit, void*) { Application::GetInstance().OnListen(start, limit); };
    static agent_link_config_t config{};
    config.device_name = app.Name();
    config.model = app.Model();
    config.caps = app.Capabilities();
    config.output = &output;
    config.platform = app.Platform();
    config.transport = AGENT_TRANSPORT_WIFI;
    config.on_status = [](const agent_link_status_t* status, void*) {
        if (status) Application::GetInstance().OnLinkStatus(*status);
    };
    ESP_ERROR_CHECK(agent_link_init(&config));
    ESP_ERROR_CHECK(agent_link_start());
    ESP_LOGI("Passport", "Deotaland WiFi firmware started");
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
