#include "dual_network_board.h"

#include <algorithm>
#include <utility>

#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "assets/lang_config.h"
#include "audio/codecs/box_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display/lcd_display.h"
#include "esp32_camera_legacy.h"
#include "i2c_device.h"
#include "power_manager.h"

#define TAG "ZhengchenQudou"

class Pca9557 : public I2cDevice {
public:
    Pca9557(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x03, 0x58);
        WriteReg(0x01, 0x03);
        SetOutputState(5, 0);
    }

    void SetOutputState(uint8_t bit, uint8_t level) {
        uint8_t data = ReadReg(0x01);
        data = (data & ~(1 << bit)) | (level << bit);
        WriteReg(0x01, data);
    }

    bool GetInputState(uint8_t bit) {
        return (ReadReg(0x00) & (1 << bit)) != 0;
    }
};

class Pca9557Button {
private:
    static constexpr uint32_t kPollIntervalUs = 20000;
    static constexpr uint32_t kDebounceThreshold = 2;

    Pca9557* pca9557_;
    uint8_t pin_bit_;
    bool active_high_;
    bool current_state_ = false;
    bool stable_state_ = false;
    bool is_pressed_ = false;
    uint32_t debounce_count_ = 0;
    esp_timer_handle_t timer_handle_ = nullptr;
    std::function<void()> on_click_;

    static void TimerCallback(void* arg) {
        static_cast<Pca9557Button*>(arg)->CheckState();
    }

    bool ReadPressed() const {
        bool pin_state = pca9557_->GetInputState(pin_bit_);
        return active_high_ ? pin_state : !pin_state;
    }

    void CheckState() {
        current_state_ = ReadPressed();
        if (current_state_ != stable_state_) {
            debounce_count_++;
            if (debounce_count_ < kDebounceThreshold) {
                return;
            }

            stable_state_ = current_state_;
            debounce_count_ = 0;
            if (stable_state_) {
                is_pressed_ = true;
            } else if (is_pressed_) {
                is_pressed_ = false;
                if (on_click_) {
                    on_click_();
                }
            }
            return;
        }

        debounce_count_ = 0;
    }

public:
    Pca9557Button(Pca9557* pca9557, uint8_t pin_bit, bool active_high = false)
        : pca9557_(pca9557), pin_bit_(pin_bit), active_high_(active_high) {
        stable_state_ = ReadPressed();
        current_state_ = stable_state_;

        esp_timer_create_args_t timer_args = {
            .callback = TimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "pca9557_button",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, kPollIntervalUs));
    }

    ~Pca9557Button() {
        if (timer_handle_ != nullptr) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
    }

    void OnClick(std::function<void()> callback) {
        on_click_ = std::move(callback);
    }
};

class CustomAudioCodec : public BoxAudioCodec {
private:
    Pca9557* pca9557_;

public:
    CustomAudioCodec(i2c_master_bus_handle_t i2c_bus, Pca9557* pca9557)
        : BoxAudioCodec(
              i2c_bus,
              AUDIO_INPUT_SAMPLE_RATE,
              AUDIO_OUTPUT_SAMPLE_RATE,
              AUDIO_I2S_GPIO_MCLK,
              AUDIO_I2S_GPIO_BCLK,
              AUDIO_I2S_GPIO_WS,
              AUDIO_I2S_GPIO_DOUT,
              AUDIO_I2S_GPIO_DIN,
              GPIO_NUM_NC,
              AUDIO_CODEC_ES8311_ADDR,
              AUDIO_CODEC_ES7210_ADDR,
              AUDIO_INPUT_REFERENCE),
          pca9557_(pca9557) {}

    void EnableOutput(bool enable) override {
        BoxAudioCodec::EnableOutput(enable);
        pca9557_->SetOutputState(1, enable ? 1 : 0);
    }
};

class ZhengchenQudouBoard : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    LcdDisplay* display_ = nullptr;
    Pca9557* pca9557_ = nullptr;
    PowerManager* power_manager_ = nullptr;
    Esp32CameraLegacy* camera_ = nullptr;
    Button boot_button_;
    Button cam_button_;
    Button vib_button_;
    Pca9557Button* volume_up_button_ = nullptr;
    Pca9557Button* volume_down_button_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
        pca9557_ = new Pca9557(i2c_bus_, 0x19);
    }

    void InitializePowerManager() {
        power_manager_ = new PowerManager([this]() -> bool {
            return !pca9557_->GetInputState(6);
        });
        power_manager_->OnBatteryShutdownRequest([this]() {
            ESP_LOGW(TAG, "Battery is empty, powering off via PCA9557 IO5");
            pca9557_->SetOutputState(5, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            pca9557_->SetOutputState(5, 0);
        });
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_SPI_MISO_PIN;
        buscfg.sclk_io_num = DISPLAY_SPI_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RESET_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        pca9557_->SetOutputState(0, 0);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new SpiLcdDisplay(
            panel_io,
            panel,
            DISPLAY_WIDTH,
            DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X,
            DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X,
            DISPLAY_MIRROR_Y,
            DISPLAY_SWAP_XY);
    }

    void InitializeCamera() {
        pca9557_->SetOutputState(2, 0);

        camera_config_t config = {};
        config.ledc_channel = LEDC_CHANNEL_2;
        config.ledc_timer = LEDC_TIMER_2;
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = -1;
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.sccb_i2c_port = 1;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 9;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        camera_ = new Esp32CameraLegacy(config);
        camera_->SetHMirror(true);
        camera_->SetVFlip(true);
    }

    void AdjustVolume(int delta) {
        auto codec = GetAudioCodec();
        int volume = std::clamp(codec->output_volume() + delta, 0, 100);
        codec->SetOutputVolume(volume);
        GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    }

    void RequestPhotoExplain() {
        static constexpr const char* kPhotoExplainPrompt = "这是什么？";

        auto* app = &Application::GetInstance();
        auto state = app->GetDeviceState();
        if (state == kDeviceStateStarting || state == kDeviceStateWifiConfiguring) {
            GetDisplay()->ShowNotification(Lang::Strings::CONNECTING);
            return;
        }

        if (state == kDeviceStateIdle) {
            app->ToggleChatState();
            app->Schedule([app]() {
                app->PhotoExplain(kPhotoExplainPrompt, "", false);
            });
            return;
        }

        if (state == kDeviceStateConnecting) {
            app->Schedule([app]() {
                app->PhotoExplain(kPhotoExplainPrompt, "", false);
            });
            return;
        }

        app->PhotoExplain(kPhotoExplainPrompt, "", false);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (GetNetworkType() == NetworkType::WIFI && app.GetDeviceState() == kDeviceStateStarting) {
                static_cast<WifiBoard&>(GetCurrentBoard()).EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        boot_button_.OnLongPress([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting ||
                app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                SwitchNetworkType();
            }
        });

        cam_button_.OnClick([this]() {
            RequestPhotoExplain();
            return;
#if 0
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting ||
                app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                GetDisplay()->ShowNotification(Lang::Strings::CONNECTING);
                return;
            }

            if (app.GetDeviceState() == kDeviceStateListening ||
                app.GetDeviceState() == kDeviceStateSpeaking) {
                app.SetDeviceState(kDeviceStateIdle);
                vTaskDelay(pdMS_TO_TICKS(200));
            }

            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.PhotoExplain("这是什么？", "", false);
            }
#endif
        });

        vib_button_.OnClick([]() {
            ESP_LOGI(TAG, "Vibration button clicked");
        });
    }

    void InitializeVolumeButtons() {
        volume_up_button_ = new Pca9557Button(pca9557_, 3, false);
        volume_down_button_ = new Pca9557Button(pca9557_, 4, false);

        volume_up_button_->OnClick([this]() {
            AdjustVolume(10);
        });

        volume_down_button_->OnClick([this]() {
            AdjustVolume(-10);
        });
    }

public:
    ZhengchenQudouBoard()
        : DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN),
          boot_button_(BOOT_BUTTON_GPIO),
          cam_button_(CAM_BUTTON_GPIO),
          vib_button_(VIB_BUTTON_GPIO) {
        InitializeI2c();
        InitializePowerManager();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeVolumeButtons();
        InitializeCamera();
        GetBacklight()->RestoreBrightness();
    }

    AudioCodec* GetAudioCodec() override {
        static CustomAudioCodec audio_codec(i2c_bus_, pca9557_);
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    Camera* GetCamera() override {
        return camera_;
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    bool GetTemperature(float& esp32temp) override {
        esp32temp = power_manager_->GetTemperature();
        return true;
    }
};

DECLARE_BOARD(ZhengchenQudouBoard);
