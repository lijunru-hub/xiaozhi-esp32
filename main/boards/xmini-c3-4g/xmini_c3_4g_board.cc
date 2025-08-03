#include "nt26_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "settings.h"
#include "config.h"
#include "sleep_timer.h"
#include "font_awesome_symbols.h"
#include "adc_battery_monitor.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "XminiC3Board"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);


class XminiC3Board : public Nt26Board {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    bool press_to_talk_enabled_ = false;
    SleepTimer* sleep_timer_ = nullptr;
    // AdcBatteryMonitor* adc_battery_monitor_ = nullptr;

    // void InitializeBatteryMonitor() {
    //     adc_battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_9, 100000, 100000, GPIO_NUM_12);
    //     adc_battery_monitor_->OnChargingStatusChanged([this](bool is_charging) {
    //         if (is_charging) {
    //             sleep_timer_->SetEnabled(false);
    //         } else {
    //             sleep_timer_->SetEnabled(true);
    //         }
    //     });
    // }

//     void InitializePowerSaveTimer() {
// #if CONFIG_USE_ESP_WAKE_WORD
//         sleep_timer_ = new SleepTimer(300);
// #else
//         sleep_timer_ = new SleepTimer(30);
// #endif
//         sleep_timer_->OnEnterLightSleepMode([this]() {
//             ESP_LOGI(TAG, "Enabling sleep mode");
//             // Show the standby screen
//             GetDisplay()->SetPowerSaveMode(true);
//             // Enable sleep mode, and sleep in 1 second after DTR is set to high
//             modem_->SetSleepMode(true, 1);
//             // Set the DTR pin to high to make the modem enter sleep mode
//             modem_->GetAtUart()->SetDtrPin(true);
//         });
//         sleep_timer_->OnExitLightSleepMode([this]() {
//             // Set the DTR pin to low to make the modem wake up
//             modem_->GetAtUart()->SetDtrPin(false);
//             // Hide the standby screen
//             GetDisplay()->SetPowerSaveMode(false);
//         });
//         sleep_timer_->SetEnabled(true);
//     }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));

        if (i2c_master_probe(codec_i2c_bus_, 0x18, 1000) != ESP_OK) {
            while (true) {
                ESP_LOGE(TAG, "Failed to probe I2C bus, please check if you have installed the correct firmware");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (!press_to_talk_enabled_) {
                app.ToggleChatState();
            }
        });
        boot_button_.OnPressDown([this]() {
            if (press_to_talk_enabled_) {
                Application::GetInstance().StartListening();
            }
        });
        boot_button_.OnPressUp([this]() {
            if (press_to_talk_enabled_) {
                Application::GetInstance().StopListening();
            }
        });
    }

    void InitializeTools() {
        Settings settings("vendor");
        press_to_talk_enabled_ = settings.GetInt("press_to_talk", 0) != 0;

        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.set_press_to_talk",
            "Switch between press to talk mode (长按说话) and click to talk mode (单击说话).\n"
            "The mode can be `press_to_talk` or `click_to_talk`.",
            PropertyList({
                Property("mode", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                auto mode = properties["mode"].value<std::string>();
                if (mode == "press_to_talk") {
                    SetPressToTalkEnabled(true);
                    return true;
                } else if (mode == "click_to_talk") {
                    SetPressToTalkEnabled(false);
                    return true;
                }
                throw std::runtime_error("Invalid mode: " + mode);
            });
    }

public:
    iot_uart_eth_config_t cfg = {
        .uart_num = UART_NUM_1,
        .baud_rate = 3000000,
        .tx_io_num = 2,
        .rx_io_num = 0,
        .rx_buffer_size = 2048,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .mrdy_io_num = 1,
        .srdy_io_num = 3,
    };
    XminiC3Board() : Nt26Board(cfg),
        boot_button_(BOOT_BUTTON_GPIO, false, 0, 0, true) {

        // InitializeBatteryMonitor();
        // InitializePowerSaveTimer();
        InitializeCodecI2c();
        InitializeButtons();
        InitializeTools();
    }

    // virtual Led* GetLed() override {
    //     static SingleLed led(BUILTIN_LED_GPIO);
    //     return &led;
    // }

    // virtual Display* GetDisplay() override {
    //     return display_;
    // }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    // virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
    //     charging = adc_battery_monitor_->IsCharging();
    //     discharging = adc_battery_monitor_->IsDischarging();
    //     level = adc_battery_monitor_->GetBatteryLevel();
    //     return true;
    // }

    void SetPressToTalkEnabled(bool enabled) {
        press_to_talk_enabled_ = enabled;

        Settings settings("vendor", true);
        settings.SetInt("press_to_talk", enabled ? 1 : 0);
        ESP_LOGI(TAG, "Press to talk enabled: %d", enabled);
    }

    bool IsPressToTalkEnabled() {
        return press_to_talk_enabled_;
    }

    // virtual void SetPowerSaveMode(bool enabled) override {
    //     if (!enabled) {
    //         sleep_timer_->WakeUp();
    //     }
    //     Nt26Board::SetPowerSaveMode(enabled);
    // }
};

DECLARE_BOARD(XminiC3Board);
