#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// vb6824
#define CODEC_TX_GPIO GPIO_NUM_18
#define CODEC_RX_GPIO GPIO_NUM_17

// gc2145 dvp摄像头
#define CAM_D0_GPIO GPIO_NUM_13
#define CAM_D1_GPIO GPIO_NUM_15
#define CAM_D2_GPIO GPIO_NUM_16
#define CAM_D3_GPIO GPIO_NUM_14
#define CAM_D4_GPIO GPIO_NUM_12
#define CAM_D5_GPIO GPIO_NUM_10
#define CAM_D6_GPIO GPIO_NUM_9
#define CAM_D7_GPIO GPIO_NUM_7
#define CAM_MCLK_GPIO GPIO_NUM_8
#define CAM_PCLK_GPIO GPIO_NUM_11
#define CAM_SCL_GPIO GPIO_NUM_4
#define CAM_SDA_GPIO GPIO_NUM_0
#define CAM_HSYNC_GPIO GPIO_NUM_6
#define CAM_VSYNC_GPIO GPIO_NUM_5

// 关机用的引脚，和adc引脚一样，但是用作输出，输出1khz关机
#define POWER_OFF_GPIO GPIO_NUM_1
#define POWER_OFF_LEDC_CHANNEL LEDC_CHANNEL_1
#define POWER_OFF_LEDC_TIMER LEDC_TIMER_1

// ADC按键(gpio1即ADC1_CH0)
#define ADC_KEY_UNIT_ID ADC_UNIT_1
#define ADC_KEY_CHANEL ADC_CHANNEL_0

// 侧面按下 0 0～1.1
// 相机    5.1/（5.1）=0.5  1.65 1.101~1.925
// 上     （5.1+5.1）/（5.1+5.1+5.1） =2/3=0.6667 2.2 1.926~2.473
// 下     （15+5.1+5.1）/（15+5.1+5.1+5.1）=0.8317 2.745 2.474~3.018
// ADC按键按下的电压(mv)
#define KEY_ONOFF_ADC_MV 0
#define KEY_TAKE_ADC_MV 1650
#define KEY_UP_ADC_MV 2200
#define KEY_DOWN_ADC_MV 2745

// tf卡
#define TF_CD_GPIO GPIO_NUM_21
#define TF_CS_GPIO GPIO_NUM_39
#define TF_SCLK_GPIO GPIO_NUM_38
#define TF_DO_GPIO GPIO_NUM_47
#define TF_DI_GPIO GPIO_NUM_48

// 屏幕st7789p3
#define LCD_SDA_GPIO GPIO_NUM_46
#define LCD_RESET_GPIO GPIO_NUM_45
#define LCD_CLK_GPIO GPIO_NUM_42
#define LCD_CD_GPIO GPIO_NUM_41
// 背光(pwm)
#define LCD_BL_EN_GPIO GPIO_NUM_40

#define DISPLAY_INVERT_COLOR true
#define DISPLAY_SWAP_XY true
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_WIDTH 296
#define DISPLAY_HEIGHT 240
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// 4G
#define TX_4G_GPIO GPIO_NUM_44
#define RX_4G_GPIO GPIO_NUM_43

// 电池检测引脚
#define BAT_DEC_GPIO GPIO_NUM_2
#define BAT_CHARGE_DEC_GPIO GPIO_NUM_3

#define POWER_SLEEP_TIME 60 //60秒后降低屏幕
#define POWER_SHUTDOWN_TIME 1800 //30分钟关机

#define USE_EXTERNAL_APP

// ── Aliases used by this demo board implementation ────────────────────────────
#define CAM_HREF_GPIO       CAM_HSYNC_GPIO
#define LCD_MOSI_GPIO       LCD_SDA_GPIO
#define LCD_DC_GPIO         LCD_CD_GPIO
#define LCD_BL_GPIO         LCD_BL_EN_GPIO

#define ADC_BTN_UNIT_ID     ADC_KEY_UNIT_ID
#define ADC_BTN_CHANNEL     ADC_KEY_CHANEL

// ESP32 UART TX drives ML307 RX (GPIO43); ML307 TX drives ESP32 UART RX (GPIO44)
#define ML307_TX_GPIO       RX_4G_GPIO
#define ML307_RX_GPIO       TX_4G_GPIO

// ADC voltage ranges (derived from center values above)
// onoff:  0 – 275 mV
#define ADC_BTN_ONOFF_MIN   0
#define ADC_BTN_ONOFF_MAX   275
// home (take):  ~1650 mV center
#define ADC_BTN_HOME_MIN    1101
#define ADC_BTN_HOME_MAX    1924
// up:     ~2200 mV center
#define ADC_BTN_UP_MIN      1925
#define ADC_BTN_UP_MAX      2473
// down:   ~2745 mV center
#define ADC_BTN_DOWN_MIN    2474
#define ADC_BTN_DOWN_MAX    3018

#endif // _BOARD_CONFIG_H_
