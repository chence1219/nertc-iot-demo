#pragma once

#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include <driver/gpio.h>
#include <driver/temperature_sensor.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "config.h"

class PowerManager {
private:
    esp_timer_handle_t timer_handle_ = nullptr;
    std::function<void(bool)> on_charging_status_changed_;
    std::function<void(bool)> on_low_battery_status_changed_;
    std::function<void(float)> on_temperature_changed_;
    std::function<void()> on_battery_shutdown_request_;

    gpio_num_t charging_pin_ = CHARGER_DETECT_PIN;
    std::function<bool()> charging_status_read_func_;
    bool use_pca9557_ = false;
    std::vector<uint16_t> adc_values_;
    std::vector<uint16_t> ref_adc_values_;
    uint32_t battery_level_ = 0;
    float current_battery_voltage_ = 0.0f;
    bool is_charging_ = false;
    bool is_low_battery_ = false;
    float current_temperature_ = 0.0f;
    int ticks_ = 0;
    int zero_battery_seconds_ = 0;

    static constexpr int kBatteryAdcInterval = 60;
    static constexpr int kBatteryAdcDataCount = 3;
    static constexpr int kLowBatteryLevel = 5;
    static constexpr int kTemperatureReadInterval = 10;
    static constexpr float kRefVoltage = 1.24f;
    static constexpr float kBatteryMinVoltage = 3.4f;
    static constexpr float kBatteryMaxVoltage = 4.15f;
    static constexpr float kVoltageDividerRatio = 2.0f;

    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    temperature_sensor_handle_t temp_sensor_ = nullptr;

    void CheckBatteryStatus() {
        bool new_charging_status = false;
        if (use_pca9557_ && charging_status_read_func_) {
            new_charging_status = charging_status_read_func_();
        } else if (charging_pin_ != GPIO_NUM_NC) {
            new_charging_status = gpio_get_level(charging_pin_) == 1;
        }

        if (new_charging_status != is_charging_) {
            is_charging_ = new_charging_status;
            if (on_charging_status_changed_) {
                on_charging_status_changed_(is_charging_);
            }
            ReadBatteryAdcData();
            return;
        }

        if (adc_values_.size() < kBatteryAdcDataCount) {
            ReadBatteryAdcData();
            return;
        }

        ticks_++;
        if (ticks_ % kBatteryAdcInterval == 0) {
            ReadBatteryAdcData();
        }

        if (ticks_ % kTemperatureReadInterval == 0) {
            ReadTemperature();
        }

        if (!is_charging_ && battery_level_ == 0) {
            zero_battery_seconds_++;
        } else {
            zero_battery_seconds_ = 0;
        }

        if (zero_battery_seconds_ >= 5 && on_battery_shutdown_request_) {
            ESP_LOGW("PowerManager", "Battery level is 0%% for %d seconds, requesting shutdown", zero_battery_seconds_);
            on_battery_shutdown_request_();
        }
    }

    void ReadBatteryAdcData() {
        int ref_adc_value = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, BATTERY_REF_ADC_CHANNEL, &ref_adc_value));

        int battery_adc_value = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, BATTERY_LEVEL_ADC_CHANNEL, &battery_adc_value));

        ref_adc_values_.push_back(ref_adc_value);
        if (ref_adc_values_.size() > kBatteryAdcDataCount) {
            ref_adc_values_.erase(ref_adc_values_.begin());
        }

        adc_values_.push_back(battery_adc_value);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }

        if (ref_adc_value <= 0) {
            ESP_LOGW("PowerManager", "Reference ADC value is zero, cannot calculate battery voltage");
            return;
        }

        uint32_t average_ref_adc = ref_adc_value;
        uint32_t average_battery_adc = battery_adc_value;
        if (adc_values_.size() >= kBatteryAdcDataCount && ref_adc_values_.size() >= kBatteryAdcDataCount) {
            average_ref_adc = 0;
            average_battery_adc = 0;
            for (auto value : ref_adc_values_) {
                average_ref_adc += value;
            }
            for (auto value : adc_values_) {
                average_battery_adc += value;
            }
            average_ref_adc /= ref_adc_values_.size();
            average_battery_adc /= adc_values_.size();
        }

        float battery_voltage =
            (static_cast<float>(average_battery_adc) / static_cast<float>(average_ref_adc)) *
            kRefVoltage * kVoltageDividerRatio;
        current_battery_voltage_ = battery_voltage;

        if (battery_voltage <= kBatteryMinVoltage) {
            battery_level_ = 0;
        } else if (battery_voltage >= kBatteryMaxVoltage) {
            battery_level_ = 100;
        } else {
            float ratio = (battery_voltage - kBatteryMinVoltage) / (kBatteryMaxVoltage - kBatteryMinVoltage);
            battery_level_ = static_cast<uint32_t>(ratio * 100.0f);
            if (battery_level_ > 100) {
                battery_level_ = 100;
            }
        }

        ESP_LOGI(
            "PowerManager",
            "Ref ADC: %lu, Battery ADC: %lu, Battery Voltage: %.2fV, Level: %lu%%",
            average_ref_adc,
            average_battery_adc,
            battery_voltage,
            battery_level_);

        if (adc_values_.size() >= kBatteryAdcDataCount) {
            bool new_low_battery_status = battery_level_ <= kLowBatteryLevel;
            if (new_low_battery_status != is_low_battery_) {
                is_low_battery_ = new_low_battery_status;
                if (on_low_battery_status_changed_) {
                    on_low_battery_status_changed_(is_low_battery_);
                }
            }
        }
    }

    void ReadTemperature() {
        float temperature = 0.0f;
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor_, &temperature));

        if (std::fabs(temperature - current_temperature_) >= 3.5f) {
            current_temperature_ = temperature;
            if (on_temperature_changed_) {
                on_temperature_changed_(current_temperature_);
            }
            ESP_LOGI("PowerManager", "Temperature updated: %.1fC", current_temperature_);
        }
    }

    void InitializeCommon() {
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                static_cast<PowerManager*>(arg)->CheckBatteryStatus();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_check_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));

        adc_oneshot_unit_init_cfg_t init_config = {};
        init_config.unit_id = ADC_UNIT_1;
        init_config.ulp_mode = ADC_ULP_MODE_DISABLE;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));

        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, BATTERY_LEVEL_ADC_CHANNEL, &chan_config));
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, BATTERY_REF_ADC_CHANNEL, &chan_config));

        temperature_sensor_config_t temp_config = {
            .range_min = 10,
            .range_max = 80,
            .clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT,
        };
        ESP_ERROR_CHECK(temperature_sensor_install(&temp_config, &temp_sensor_));
        ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor_));
    }

public:
    explicit PowerManager(gpio_num_t pin) : charging_pin_(pin), use_pca9557_(false) {
        if (pin != GPIO_NUM_NC) {
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pin_bit_mask = (1ULL << charging_pin_);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf);
        }
        InitializeCommon();
    }

    explicit PowerManager(std::function<bool()> charging_status_read_func)
        : charging_pin_(GPIO_NUM_NC),
          charging_status_read_func_(std::move(charging_status_read_func)),
          use_pca9557_(true) {
        InitializeCommon();
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (adc_handle_) {
            adc_oneshot_del_unit(adc_handle_);
        }
        if (temp_sensor_) {
            temperature_sensor_disable(temp_sensor_);
            temperature_sensor_uninstall(temp_sensor_);
        }
    }

    bool IsCharging() const {
        return is_charging_;
    }

    bool IsDischarging() const {
        return !is_charging_;
    }

    uint8_t GetBatteryLevel() const {
        return battery_level_;
    }

    float GetBatteryVoltage() const {
        return current_battery_voltage_;
    }

    float GetTemperature() const {
        return current_temperature_;
    }

    void OnTemperatureChanged(std::function<void(float)> callback) {
        on_temperature_changed_ = std::move(callback);
    }

    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) {
        on_low_battery_status_changed_ = std::move(callback);
    }

    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = std::move(callback);
    }

    void OnBatteryShutdownRequest(std::function<void()> callback) {
        on_battery_shutdown_request_ = std::move(callback);
    }
};
