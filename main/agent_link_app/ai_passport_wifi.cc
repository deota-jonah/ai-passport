#include "application.h"
#include "ai_passport_hardware.h"
#include "agent_link_platform.h"
#include "passport_config.h"
#include "passport_language.h"
#include "passport_wifi_audio.h"
#include "passport_wifi_controls.h"
#include "passport_wifi_ui.h"
#include "stripe_canvas.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_system.h"
extern "C" {
#include "bsp_button.h"
}

namespace {
constexpr const char* TAG = "Passport.wifi";
using passport_wifi::Gesture;
using passport_wifi::TextId;
using passport_wifi::Tr;
using passport_wifi::Language;
enum class Page { Home, Menu, Sessions, ConfirmWifi, Language };
enum class Action { Sessions, New, Back, Select, Previous, Next, Wifi, ClearWifi, Language, Chinese, English };
struct Item { Action action; unsigned row; char name[64]; };
struct Input { Gesture gesture; uint32_t view; };
}

class AiPassportWifi : public Application {
public:
    AiPassportWifi() {
        ESP_ERROR_CHECK(nvs_flash_init());
        Load();
        hardware_.Init();
        if (hardware_.audio_ready) (void)hardware_.codec.SetVolume(volume_.load());
        if (audio_.Init(&hardware_) != ESP_OK) ESP_LOGE(TAG, "Audio unavailable");
        if (hardware_.display_ready && passport_wifi::StartUi(&hardware_.lcd) != ESP_OK)
            ESP_LOGE(TAG, "UI unavailable");
        events_ = xQueueCreate(8, sizeof(Input));
        ESP_ERROR_CHECK(events_ ? ESP_OK : ESP_ERR_NO_MEM);
        ESP_ERROR_CHECK(xTaskCreate(ControlEntry, "passport_ctrl", 6144, this, 4, nullptr) == pdPASS
                        ? ESP_OK : ESP_ERR_NO_MEM);
        hardware_.keys_ready = bsp_button_init(KeyEvent, this) == ESP_OK;
        ESP_LOGI(TAG, "WiFi voice ready: product=%d, internal free=%u largest=%u",
                 AGENT_LINK_PRODUCT_ID,
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    }
    const char* Name() const override { return "DeotaPassport"; }
    const char* Model() const override { return "AI-PASSPORT"; }
    const agent_platform_t* Platform() const override {
        static const agent_platform_t platform = {
            .base_url = DEOTALAND_PLATFORM_BASE_URL, .product_id = AGENT_LINK_PRODUCT_ID,
            .chip_type = "esp32-c3",
        };
        return &platform;
    }
    uint32_t Capabilities() const override {
        uint32_t caps = hardware_.keys_ready ? AGENT_CAP_BUTTON : 0;
        if (hardware_.display_ready) caps |= AGENT_CAP_SCREEN;
        if (audio_.Ready()) caps |= AGENT_CAP_MIC | AGENT_CAP_SPEAKER;
        return caps;
    }
    void OnLinkStatus(const agent_link_status_t& status) override { passport_wifi::SetStatus(status); }
    void PlayAudio(const uint8_t* pcm, size_t bytes) override { audio_.Play(pcm, bytes); }
    void AudioEnd() override { audio_.End(); }
    void AudioReset() override { audio_.Reset(); }
    void ShowText(const char* text) override { passport_wifi::SetAnswer(text); }
    void OnListen(bool start, uint32_t) override {
        if (!start) audio_.Enable(false);
    }
private:
    static void ControlEntry(void* ctx) { static_cast<AiPassportWifi*>(ctx)->ControlLoop(); }
    static void KeyEvent(bsp_btn_t button, bsp_btn_ev_t event, void* ctx) {
        auto* app = static_cast<AiPassportWifi*>(ctx);
        if (button == BSP_BTN_OK && event == BSP_BTN_PRESS) app->held_ok_ = false;
        Gesture gesture = Gesture::None;
        if (button == BSP_BTN_OK && event == BSP_BTN_LONG) {
            app->held_ok_ = true; gesture = Gesture::Hold;
        } else if (event == BSP_BTN_CLICK) {
            if (button == BSP_BTN_OK && !app->held_ok_) gesture = Gesture::Click;
            else if (button == BSP_BTN_UP) gesture = Gesture::Up;
            else if (button == BSP_BTN_DOWN) gesture = Gesture::Down;
        }
        if (gesture != Gesture::None) {
            const Input input{gesture, app->view_.load()};
            (void)xQueueSend(app->events_, &input, 0);
        }
    }
    void Load() {
        nvs_handle_t handle;
        if (nvs_open("passport_wifi", NVS_READONLY, &handle) != ESP_OK) return;
        (void)nvs_get_i32(handle, "agent", &saved_agent_);
        size_t size = sizeof session_; (void)nvs_get_str(handle, "session", session_, &size);
        size = sizeof session_name_; (void)nvs_get_str(handle, "name", session_name_, &size);
        uint8_t volume = 70; (void)nvs_get_u8(handle, "volume", &volume);
        volume_.store(volume > 100 ? 100 : volume);
        uint8_t language = 0; (void)nvs_get_u8(handle, "language", &language);
        passport_wifi::SetLanguage(passport_wifi::StoredLanguage(language));
        nvs_close(handle);
        if (saved_agent_ <= 0) session_[0] = session_name_[0] = 0;
    }
    void Save() {
        nvs_handle_t handle;
        if (nvs_open("passport_wifi", NVS_READWRITE, &handle) != ESP_OK) return;
        esp_err_t result = nvs_set_i32(handle, "agent", saved_agent_);
        if (result == ESP_OK) result = nvs_set_str(handle, "session", session_);
        if (result == ESP_OK) result = nvs_set_str(handle, "name", session_name_);
        if (result == ESP_OK) result = nvs_set_u8(handle, "volume", volume_.load());
        if (result == ESP_OK) result = nvs_set_u8(handle, "language", static_cast<uint8_t>(passport_wifi::GetLanguage()));
        if (result == ESP_OK) result = nvs_commit(handle);
        nvs_close(handle);
        if (result != ESP_OK) ESP_LOGW(TAG, "Settings save failed: %s", esp_err_to_name(result));
    }
    void EndCall() {
        calling_ = active_ = false;
        requested_at_ = 0;
        audio_.Stop();
        (void)agent_link_end_realtime();
    }
    void Error(esp_err_t result) {
        raster::CopyText(message_, sizeof message_, Tr(TextId::RequestFailed));
        ESP_LOGW(TAG, "Platform request: %s", esp_err_to_name(result));
    }
    bool SyncAgent(bool explain = false) {
        agent_link_platform_agent_t bound{};
        const esp_err_t result = agent_link_platform_bound_agent(&bound);
        if (result != ESP_OK) {
            if (agent_) { EndCall(); agent_ = 0; }
            if (explain) raster::CopyText(message_, sizeof message_, Tr(TextId::ActivateFirst));
            return false;
        }
        if (agent_ != bound.id) {
            EndCall(); agent_ = bound.id;
            if (saved_agent_ != bound.id) {
                session_[0] = session_name_[0] = 0;
                saved_agent_ = bound.id; Save();
            }
            if (page_ == Page::Sessions) SetPage(Page::Home);
        }
        voice_supported_ = bound.voice_supported;
        raster::CopyText(agent_name_, sizeof agent_name_, bound.name);
        if (explain && !voice_supported_)
            raster::CopyText(message_, sizeof message_, Tr(TextId::NoVoice));
        return voice_supported_;
    }
    void SetPage(Page page) {
        page_ = page; count_ = cursor_ = 0; message_[0] = 0;
        menu_.store(page != Page::Home); view_.fetch_add(1);
    }
    void Add(Action action, const char* name, unsigned row = 0) {
        if (count_ >= 6) return;
        Item& item = items_[count_++]; item.action = action; item.row = row;
        raster::CopyText(item.name, sizeof item.name, name);
    }
    void Menu() {
        EndCall(); SetPage(Page::Menu); passport_wifi::SetAnswer("");
        Add(Action::Sessions, Tr(TextId::ChooseConversation));
        Add(Action::New, Tr(TextId::NewConversation));
        Add(Action::Wifi, Tr(TextId::ChangeWifi));
        Add(Action::Language, Tr(TextId::LanguageTitle));
        Add(Action::Back, Tr(TextId::Back));
    }
    void LanguageMenu() {
        SetPage(Page::Language);
        Add(Action::Chinese, Tr(TextId::ChineseOption));
        Add(Action::English, Tr(TextId::EnglishOption));
        Add(Action::Back, Tr(TextId::Back));
    }
    bool Fetch(size_t offset) {
        size_t count = 0, total = 0;
        const esp_err_t result = agent_link_platform_conversations(agent_, offset, sessions_, 3, &count, &total);
        if (result != ESP_OK) { Error(result); return false; }
        fetched_ = count; offset_ = offset; total_ = total;
        return true;
    }
    void Sessions(size_t offset = 0) {
        if (!SyncAgent(true)) return;
        EndCall(); Busy(Tr(TextId::Loading));
        if (!Fetch(offset)) return;
        if (!fetched_ && offset && !Fetch(0)) return;
        SetPage(Page::Sessions);
        for (size_t i = 0; i < fetched_; ++i)
            Add(Action::Select, sessions_[i].name[0] ? sessions_[i].name : Tr(TextId::Conversation), i);
        if (offset_) Add(Action::Previous, Tr(TextId::Previous));
        if (total_ > offset_ + fetched_) Add(Action::Next, Tr(TextId::Next));
        Add(Action::Back, Tr(TextId::Back));
        if (!fetched_) raster::CopyText(message_, sizeof message_, Tr(TextId::NoConversations));
    }
    void Busy(const char* title) {
        view_.fetch_add(1);
        passport_wifi::SetContent(title, Tr(TextId::PleaseWait), Tr(TextId::VolumeKeys));
    }
    void SelectSession(size_t row) {
        snprintf(session_, sizeof session_, "%s", sessions_[row].id);
        raster::CopyText(session_name_, sizeof session_name_, sessions_[row].name);
        saved_agent_ = agent_; Save(); SetPage(Page::Home);
    }
    bool CreateSession() {
        char id[sizeof session_]{};
        const esp_err_t result = agent_link_platform_create_conversation(agent_, id, sizeof id);
        if (result != ESP_OK) { Error(result); return false; }
        snprintf(session_, sizeof session_, "%s", id);
        raster::CopyText(session_name_, sizeof session_name_, Tr(TextId::NewConversation));
        saved_agent_ = agent_; Save(); return true;
    }
    void StartCall() {
        if (!audio_.Ready()) { raster::CopyText(message_, sizeof message_, Tr(TextId::AudioUnavailable)); return; }
        if (!SyncAgent(true)) return;
        if (agent_link_state() != AGENT_STATE_CONNECTED) {
            raster::CopyText(message_, sizeof message_, Tr(TextId::WaitingConnection)); return;
        }
        Busy(Tr(TextId::PreparingVoice));
        if (!session_[0]) {
            if (!Fetch(0)) return;
            if (fetched_) SelectSession(0);
            else if (!CreateSession()) return;
        }
        // APIs run on this worker. Recheck binding after HTTPS, before requesting a grant.
        agent_link_platform_agent_t bound{};
        if (agent_link_platform_bound_agent(&bound) != ESP_OK || bound.id != agent_) {
            raster::CopyText(message_, sizeof message_, Tr(TextId::BindingChanged)); return;
        }
        const esp_err_t result = agent_link_request_realtime_session(agent_, session_);
        if (result != ESP_OK) { Error(result); return; }
        passport_wifi::SetAnswer(""); message_[0] = 0;
        calling_ = true; active_ = false; requested_at_ = esp_timer_get_time();
    }
    void Handle(Gesture gesture) {
        message_[0] = 0;
        if (gesture == Gesture::Hold) { if (page_ == Page::Home) Menu(); else SetPage(Page::Home); return; }
        if (page_ == Page::Home) {
            if (gesture == Gesture::Up || gesture == Gesture::Down) {
                int volume = volume_.load() + (gesture == Gesture::Up ? 5 : -5);
                volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
                volume_.store(volume);
                if (hardware_.audio_ready) (void)hardware_.codec.SetVolume(volume);
                return;
            }
            if (gesture == Gesture::Click) { if (calling_) EndCall(); else StartCall(); }
            return;
        }
        if (gesture == Gesture::Up && cursor_) --cursor_;
        else if (gesture == Gesture::Down && cursor_ + 1 < count_) ++cursor_;
        else if (gesture == Gesture::Click && count_) {
            const Item item = items_[cursor_];
            switch (item.action) {
            case Action::Sessions: Sessions(); break;
            case Action::New:
                if (SyncAgent(true)) { Busy(Tr(TextId::Creating)); if (CreateSession()) SetPage(Page::Home); }
                break;
            case Action::Language: LanguageMenu(); break;
            case Action::Chinese:
            case Action::English:
                passport_wifi::SetLanguage(item.action == Action::English ? Language::English : Language::Chinese);
                Save(); LanguageMenu(); break;
            case Action::Wifi:
                SetPage(Page::ConfirmWifi);
                Add(Action::Back, Tr(TextId::Cancel));
                Add(Action::ClearWifi, Tr(TextId::ClearWifi));
                break;
            case Action::ClearWifi: {
                nvs_handle_t handle;
                esp_err_t result = nvs_open("al_wifi", NVS_READWRITE, &handle);
                if (result == ESP_OK) {
                    result = nvs_erase_all(handle);
                    if (result == ESP_OK) result = nvs_commit(handle);
                    nvs_close(handle);
                }
                if (result == ESP_OK) esp_restart();
                Error(result);
                break;
            }
            case Action::Back: if (page_ == Page::Sessions || page_ == Page::Language) Menu(); else SetPage(Page::Home); break;
            case Action::Select: SelectSession(item.row); break;
            case Action::Previous: Sessions(offset_ >= 3 ? offset_ - 3 : 0); break;
            case Action::Next: Sessions(offset_ + fetched_); break;
            }
        }
        view_.fetch_add(1);
    }
    void Publish() {
        char detail[384]{};
        if (page_ != Page::Home) {
            if (message_[0]) snprintf(detail, sizeof detail, "%s", message_);
            else for (size_t i = (cursor_ / 3) * 3; i < count_ && i < (cursor_ / 3) * 3 + 3; ++i) {
                const size_t used = strlen(detail);
                raster::TextLayout layout;
                layout.Wrap(raster_cjk_14, items_[i].name, 162);
                const int length = layout.Count() ? layout.End(0) : 0;
                snprintf(detail + used, sizeof detail - used, "%s %.*s%s\n\n", i == cursor_ ? ">" : " ",
                         length, items_[i].name, layout.Count() > 1 ? "..." : "");
            }
            passport_wifi::SetContent(page_ == Page::Menu ? Tr(TextId::SettingsTitle) : page_ == Page::Language ? Tr(TextId::LanguageTitle) : page_ == Page::ConfirmWifi ? Tr(TextId::ResetTitle) : Tr(TextId::ConversationsTitle), detail,
                                      Tr(TextId::MenuFooter), true);
        } else {
            if (message_[0]) snprintf(detail, sizeof detail, "%s", message_);
            else {
                char volume[40]; snprintf(volume, sizeof volume, Tr(TextId::Volume), volume_.load());
                snprintf(detail, sizeof detail, "%s\n\n%s\n\n%s",
                         agent_name_[0] ? agent_name_ : Tr(TextId::BindAgent),
                         session_[0] ? Tr(TextId::Selected) : Tr(TextId::ContinueLatest), volume);
            }
            passport_wifi::SetContent(active_ ? (audio_.Speaking() ? Tr(TextId::Speaking) : Tr(TextId::Listening)) :
                                      calling_ ? Tr(TextId::StartingCall) : Tr(TextId::HomeTitle), detail,
                                      calling_ ? Tr(TextId::CallFooter) : Tr(TextId::HomeFooter));
        }
    }
    void ControlLoop() {
        int64_t sync_at = 0;
        int saved_volume = volume_.load();
        for (;;) {
            Input input{};
            if (xQueueReceive(events_, &input, pdMS_TO_TICKS(100)) == pdTRUE && input.view == view_.load())
                Handle(input.gesture);
            const int64_t now = esp_timer_get_time();
            if (now - sync_at >= 1000000) {
                (void)SyncAgent(); sync_at = now;
                if (saved_volume != volume_.load()) { Save(); saved_volume = volume_.load(); }
            }
            if (calling_) {
                if (active_ && !audio_.Enabled()) {
                    EndCall(); raster::CopyText(message_, sizeof message_, Tr(TextId::MicrophoneStopped));
                } else if (agent_link_state() == AGENT_STATE_READY) {
                    if (!active_) { audio_.Enable(true); active_ = true; }
                } else if (active_ || agent_link_state() == AGENT_STATE_DISCONNECTED || now - requested_at_ > 30000000) {
                    EndCall(); raster::CopyText(message_, sizeof message_, Tr(TextId::CallEnded));
                }
            }
            Publish();
        }
    }
    AiPassportHardware hardware_;
    PassportWifiAudio audio_;
    QueueHandle_t events_ = nullptr;
    std::atomic<int> volume_{70};
    std::atomic<bool> menu_{false};
    std::atomic<uint32_t> view_{1};
    bool held_ok_ = false;
    Page page_ = Page::Home;
    Item items_[6]{};
    size_t count_ = 0, cursor_ = 0, fetched_ = 0, offset_ = 0, total_ = 0;
    agent_link_platform_conversation_t sessions_[3]{};
    int agent_ = 0;
    int32_t saved_agent_ = 0;
    char session_[80]{}, session_name_[96]{}, agent_name_[80]{}, message_[144]{};
    bool voice_supported_ = false, calling_ = false, active_ = false;
    int64_t requested_at_ = 0;
};

DECLARE_APPLICATION(AiPassportWifi);
