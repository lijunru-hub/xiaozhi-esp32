#include "wifi_board.h"
#include "audio_codecs/adc_pdm_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#define TAG "esp_spot_c3"

class EspSpotC3Bot : public WifiBoard {
private:
    Button boot_button_;

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            printf("Toggle chat state\n");
            app.ToggleChatState();
        });
    }

    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
    }

public:
    EspSpotC3Bot() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeButtons();
        InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override {
         static AdcPdmAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_ADC_MIC_CHANNEL, AUDIO_PDM_SPEAK_GPIO);
        return &audio_codec;
    }
};

DECLARE_BOARD(EspSpotC3Bot);