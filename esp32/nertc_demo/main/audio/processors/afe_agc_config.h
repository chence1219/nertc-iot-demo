#ifndef AFE_AGC_CONFIG_H
#define AFE_AGC_CONFIG_H

#include <algorithm>

#include <cJSON.h>
#include <esp_afe_config.h>
#include <esp_log.h>

#if CONFIG_CONNECTION_TYPE_NERTC
#include "nertc_protocol.h"
#endif

namespace afe_runtime_config {

struct AgcConfig {
    bool enabled = false;
    afe_agc_mode_t mode = AFE_AGC_MODE_WEBRTC;
    int compression_gain_db = 9;
    int target_level_dbfs = 3;
    bool loaded_from_local_config = false;
};

inline AgcConfig LoadAgcConfig() {
    AgcConfig config;

#if CONFIG_CONNECTION_TYPE_NERTC
    cJSON* config_json = NeRtcProtocol::ReadConfigJson();
    if (config_json == nullptr) {
        return config;
    }

    cJSON* audio_config = cJSON_GetObjectItem(config_json, "audio_config");
    cJSON* afe_agc_config = cJSON_IsObject(audio_config) ? cJSON_GetObjectItem(audio_config, "afe_agc") : nullptr;
    if (cJSON_IsObject(afe_agc_config)) {
        config.loaded_from_local_config = true;

        cJSON* enabled = cJSON_GetObjectItem(afe_agc_config, "enabled");
        if (cJSON_IsBool(enabled)) {
            config.enabled = cJSON_IsTrue(enabled);
        }

        cJSON* mode = cJSON_GetObjectItem(afe_agc_config, "mode");
        if (cJSON_IsNumber(mode)) {
            int raw_mode = mode->valueint;
            if (raw_mode == AFE_AGC_MODE_WEBRTC || raw_mode == AFE_AGC_MODE_WAKENET) {
                config.mode = static_cast<afe_agc_mode_t>(raw_mode);
            }
        }

        cJSON* compression_gain_db = cJSON_GetObjectItem(afe_agc_config, "compression_gain_db");
        if (cJSON_IsNumber(compression_gain_db)) {
            config.compression_gain_db = std::clamp(compression_gain_db->valueint, 0, 90);
        }

        cJSON* target_level_dbfs = cJSON_GetObjectItem(afe_agc_config, "target_level_dbfs");
        if (cJSON_IsNumber(target_level_dbfs)) {
            config.target_level_dbfs = std::clamp(target_level_dbfs->valueint, 0, 31);
        }
    }

    cJSON_Delete(config_json);
#endif

    return config;
}

inline void ApplyAgcConfig(afe_config_t* afe_config, const char* tag) {
    auto agc_config = LoadAgcConfig();
    afe_config->agc_init = agc_config.enabled;
    afe_config->agc_mode = agc_config.mode;
    afe_config->agc_compression_gain_db = agc_config.compression_gain_db;
    afe_config->agc_target_level_dbfs = agc_config.target_level_dbfs;

    ESP_LOGI(tag,
        "AFE AGC config: enabled=%d, mode=%d, compression_gain=%d dB, target_level=-%d dBFS, source=%s",
        agc_config.enabled ? 1 : 0,
        agc_config.mode,
        agc_config.compression_gain_db,
        agc_config.target_level_dbfs,
        agc_config.loaded_from_local_config ? "local_config" : "defaults");
}

}  // namespace afe_runtime_config

#endif
