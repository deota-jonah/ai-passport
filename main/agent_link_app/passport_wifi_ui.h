#pragma once
#include "agent_link.h"
#include "ai_passport_hardware.h"

namespace passport_wifi {
esp_err_t StartUi(PassportPanel* lcd);
void SetStatus(const agent_link_status_t& status);
void SetContent(const char* title, const char* detail, const char* footer, bool navigation = false);
void SetAnswer(const char* text);
}  // namespace passport_wifi
