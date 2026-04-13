#include "dual_network_board.h"
#include "wifi_board.h"
#include "audio/codecs/vb6824_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include "esp32_camera_legacy.h"

#define TAG "zuowei-gen5"

class ZuoweiGen5Board : public DualNetworkBoard {
private:
    AdcButton* btn_onoff_ = nullptr;
    AdcButton* btn_home_  = nullptr;
    AdcButton* btn_up_    = nullptr;
    AdcButton* btn_down_  = nullptr;
    adc_oneshot_unit_handle_t adc_handle_;
    Display*   display_   = nullptr;
    Esp32CameraLegacy* camera_ = nullptr;

    void ChangeVol(int delta) {
        auto codec  = GetAudioCodec();
        int  volume = codec->output_volume() + delta;
        volume = volume < 0 ? 0 : (volume > 100 ? 100 : volume);
        codec->SetOutputVolume(volume);
        GetDisplay()->ShowNotification(
            Lang::Strings::VOLUME + std::to_string(volume));
    }

    void InitializeButtons() {
        const adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = ADC_BTN_UNIT_ID,
        };
        adc_oneshot_new_unit(&init_cfg, &adc_handle_);

        button_adc_config_t adc_cfg = {};
        adc_cfg.adc_channel = ADC_BTN_CHANNEL;
        adc_cfg.adc_handle  = &adc_handle_;

        adc_cfg.button_index = 0;
        adc_cfg.min = ADC_BTN_ONOFF_MIN;
        adc_cfg.max = ADC_BTN_ONOFF_MAX;
        btn_onoff_ = new AdcButton(adc_cfg);

        adc_cfg.button_index = 1;
        adc_cfg.min = ADC_BTN_HOME_MIN;
        adc_cfg.max = ADC_BTN_HOME_MAX;
        btn_home_ = new AdcButton(adc_cfg);

        adc_cfg.button_index = 2;
        adc_cfg.min = ADC_BTN_UP_MIN;
        adc_cfg.max = ADC_BTN_UP_MAX;
        btn_up_ = new AdcButton(adc_cfg);

        adc_cfg.button_index = 3;
        adc_cfg.min = ADC_BTN_DOWN_MIN;
        adc_cfg.max = ADC_BTN_DOWN_MAX;
        btn_down_ = new AdcButton(adc_cfg);

        auto onoff_handler = [this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                if (GetNetworkType() == NetworkType::WIFI) {
                    ESP_LOGI(TAG, "btn onoff/home: starting -> enter wifi config");
                    static_cast<WifiBoard&>(GetCurrentBoard()).EnterWifiConfigMode();
                }
                return;
            }
            ESP_LOGI(TAG, "btn onoff/home: toggle chat state");
            app.ToggleChatState();
        };
        btn_onoff_->OnClick(onoff_handler);
        btn_home_->OnClick(onoff_handler);

        // 连按 4 次 onoff/home 键切换 WiFi / 4G 网络
        auto switch_net = [this]() {
            ESP_LOGI(TAG, "btn onoff/home x4: switch network type");
            SwitchNetworkType();
        };
        btn_onoff_->OnMultipleClick(switch_net, 4);
        btn_home_->OnMultipleClick(switch_net, 4);

        btn_up_->OnClick([this]() {
            ChangeVol(+10);
            ESP_LOGI(TAG, "btn up: vol +10");
        });
        btn_down_->OnClick([this]() {
            ChangeVol(-10);
            ESP_LOGI(TAG, "btn down: vol -10");
        });
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num   = LCD_MOSI_GPIO;
        buscfg.miso_io_num   = GPIO_NUM_NC;
        buscfg.sclk_io_num   = LCD_CLK_GPIO;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t    panel    = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num       = GPIO_NUM_NC;
        io_config.dc_gpio_num       = LCD_DC_GPIO;
        io_config.spi_mode          = 3;
        io_config.pclk_hz           = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits      = 8;
        io_config.lcd_param_bits    = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num   = LCD_RESET_GPIO;
        panel_config.rgb_ele_order    = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel   = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new SpiLcdDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeCamera() {
        camera_config_t cam_cfg = {};
        cam_cfg.pin_pwdn    = GPIO_NUM_NC;
        cam_cfg.pin_reset   = GPIO_NUM_NC;
        cam_cfg.pin_xclk    = CAM_MCLK_GPIO;
        cam_cfg.pin_pclk    = CAM_PCLK_GPIO;
        cam_cfg.pin_href    = CAM_HREF_GPIO;
        cam_cfg.pin_vsync   = CAM_VSYNC_GPIO;
        cam_cfg.pin_sccb_sda = CAM_SDA_GPIO;
        cam_cfg.pin_sccb_scl = CAM_SCL_GPIO;
        cam_cfg.sccb_i2c_port = I2C_NUM_0;
        cam_cfg.pin_d0 = CAM_D0_GPIO;
        cam_cfg.pin_d1 = CAM_D1_GPIO;
        cam_cfg.pin_d2 = CAM_D2_GPIO;
        cam_cfg.pin_d3 = CAM_D3_GPIO;
        cam_cfg.pin_d4 = CAM_D4_GPIO;
        cam_cfg.pin_d5 = CAM_D5_GPIO;
        cam_cfg.pin_d6 = CAM_D6_GPIO;
        cam_cfg.pin_d7 = CAM_D7_GPIO;
        cam_cfg.xclk_freq_hz  = 20 * 1000 * 1000;
        cam_cfg.ledc_timer    = LEDC_TIMER_1;
        cam_cfg.ledc_channel  = LEDC_CHANNEL_0;
        cam_cfg.pixel_format  = PIXFORMAT_RGB565;
        cam_cfg.frame_size    = FRAMESIZE_QVGA;
        cam_cfg.jpeg_quality  = 12;
        cam_cfg.fb_count      = 1;
        cam_cfg.fb_location   = CAMERA_FB_IN_PSRAM;
        cam_cfg.grab_mode     = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32CameraLegacy(cam_cfg);
    }

public:
    ZuoweiGen5Board() : DualNetworkBoard(ML307_TX_GPIO, ML307_RX_GPIO, GPIO_NUM_NC, 0), audio_codec_(CODEC_TX_GPIO, CODEC_RX_GPIO) {
        InitializeButtons();
        InitializeSpi();
        InitializeDisplay();
        InitializeCamera();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        return &audio_codec_;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(LCD_BL_GPIO, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

private:
    VbAduioCodec audio_codec_;
};

DECLARE_BOARD(ZuoweiGen5Board);
