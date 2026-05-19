#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <font_awesome.h>

#include "display.h"
#include "settings.h"

#define TAG "Display"

Display::Display() {
}

Display::~Display() {
}

void Display::SetStatus(const char* status) {
    ESP_LOGW(TAG, "SetStatus: %s", status);
}

void Display::ShowNotification(const std::string &notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void Display::ShowNotification(const char* notification, int duration_ms) {
    ESP_LOGW(TAG, "ShowNotification: %s", notification);
}

void Display::UpdateStatusBar(bool update_all) {
}


void Display::SetEmotion(const char* emotion) {
    ESP_LOGW(TAG, "SetEmotion: %s", emotion);
}

void Display::LockEmotion(bool lock) {
    ESP_LOGW(TAG, "LockEmotion: %d", lock);
}
bool Display::IsEmotionLocked(bool check) {
    ESP_LOGW(TAG, "IsEmotionLocked");
    return false;
}

void Display::SetChatMessage(const char* role, const char* content) {
    ESP_LOGW(TAG, "Role:%s", role);
    ESP_LOGW(TAG, "     %s", content);
}

void Display::SetBgImage(const lv_image_dsc_t*, bool) {
}

void Display::SetTheme(Theme* theme) {
    current_theme_ = theme;
    Settings settings("display", true);
    settings.SetString("theme", theme->name());
}

void Display::SetTheme(const std::string&) {
}

void Display::SetPowerSaveMode(bool on) {
    ESP_LOGW(TAG, "SetPowerSaveMode: %d", on);
}

void Display::SetTitleText(const std::string& text) {
    ESP_LOGW(TAG, "SetTitleText: %s", text.c_str());
}

void Display::SetStatusProfile(const std::string& profile) {
    status_profile_ = profile;
}

void Display::SetStatusText(const std::string& text) {
    ESP_LOGW(TAG, "SetStatusText: %s%s", status_profile_.c_str(), text.c_str());
}

void Display::SetVersionProfile(const std::string& profile) {
    version_profile_ = profile;
}

void Display::SetVersionText(const std::string& version) {
    ESP_LOGW(TAG, "SetVersionText: %s%s", version_profile_.c_str(), version.c_str());
}
