#include "nt26_board.h"

#include "application.h"
#include "display.h"
#include "font_awesome_symbols.h"
#include "assets/lang_config.h"
#include "iot_eth.h"
#include "iot_uart_eth.h"
#include "esp_netif.h"
#include "iot_eth_netif_glue.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <opus_encoder.h>
#include <esp_network.h>

static const char *TAG = "Nt26Board";

Nt26Board::Nt26Board(iot_uart_eth_config_t uart_eth_config) : uart_eth_config_(uart_eth_config) {
}

std::string Nt26Board::GetBoardType() {
    return "nt26";
}

void Nt26Board::StartNetwork() {
    auto& application = Application::GetInstance();
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(Lang::Strings::DETECTING_MODULE);

    if (uart_eth_handle_ == nullptr) {
        esp_err_t ret = iot_eth_new_uart_eth(&uart_eth_config_, &uart_eth_handle_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create UART ETH driver");
            display->SetStatus(Lang::Strings::ERROR);
            return;
        }

        iot_eth_config_t eth_cfg = {
            .driver = uart_eth_handle_,
            .stack_input = NULL,
            .user_data = NULL,
        };

        ret = iot_eth_install(&eth_cfg, &eth_handle_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to install UART ETH driver");
            display->SetStatus(Lang::Strings::ERROR);
            return;
        }

        esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

        iot_eth_netif_glue_handle_t glue = iot_eth_new_netif_glue(eth_handle_);
        if (glue == NULL) {
            ESP_LOGE(TAG, "Failed to create netif glue");
            return;
        }
        esp_netif_attach(eth_netif, glue);
    }

    // Wait for network ready
    display->SetStatus(Lang::Strings::REGISTERING_NETWORK);
    while (true) {
        esp_err_t ret =iot_eth_start(eth_handle_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start UART ETH driver");
            display->SetStatus(Lang::Strings::ERROR);
        } else {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelay(pdMS_TO_TICKS(10000));

    // Print the ML307 modem information
    // std::string module_revision = modem_->GetModuleRevision();
    // std::string imei = modem_->GetImei();
    // std::string iccid = modem_->GetIccid();
    // ESP_LOGI(TAG, "ML307 Revision: %s", module_revision.c_str());
    // ESP_LOGI(TAG, "ML307 IMEI: %s", imei.c_str());
    // ESP_LOGI(TAG, "ML307 ICCID: %s", iccid.c_str());
}

NetworkInterface* Nt26Board::GetNetwork() {
    static EspNetwork network;
    return &network;
}

const char* Nt26Board::GetNetworkStateIcon() {
    if (eth_handle_ == nullptr) {
        return FONT_AWESOME_SIGNAL_OFF;
    }

    return FONT_AWESOME_SIGNAL_4;
}

std::string Nt26Board::GetBoardJson() {
    std::string board_json = std::string("{\"name\":\"" BOARD_NAME "\"}");
    return board_json;
}

void Nt26Board::SetPowerSaveMode(bool enabled) {
    // TODO: Implement power save mode for ML307
}

std::string Nt26Board::GetDeviceStatusJson() {
    /*
     * 返回设备状态JSON
     * 
     * 返回的JSON结构如下：
     * {
     *     "audio_speaker": {
     *         "volume": 70
     *     },
     *     "screen": {
     *         "brightness": 100,
     *         "theme": "light"
     *     },
     *     "battery": {
     *         "level": 50,
     *         "charging": true
     *     },
     *     "network": {
     *         "type": "cellular",
     *         "carrier": "CHINA MOBILE",
     *         "csq": 10
     *     }
     * }
     */
    auto& board = Board::GetInstance();
    auto root = cJSON_CreateObject();

    // Audio speaker
    auto audio_speaker = cJSON_CreateObject();
    auto audio_codec = board.GetAudioCodec();
    if (audio_codec) {
        cJSON_AddNumberToObject(audio_speaker, "volume", audio_codec->output_volume());
    }
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

    // Screen brightness
    auto backlight = board.GetBacklight();
    auto screen = cJSON_CreateObject();
    if (backlight) {
        cJSON_AddNumberToObject(screen, "brightness", backlight->brightness());
    }
    auto display = board.GetDisplay();
    if (display && display->height() > 64) { // For LCD display only
        cJSON_AddStringToObject(screen, "theme", display->GetTheme().c_str());
    }
    cJSON_AddItemToObject(root, "screen", screen);

    // Battery
    int battery_level = 0;
    bool charging = false;
    bool discharging = false;
    if (board.GetBatteryLevel(battery_level, charging, discharging)) {
        cJSON* battery = cJSON_CreateObject();
        cJSON_AddNumberToObject(battery, "level", battery_level);
        cJSON_AddBoolToObject(battery, "charging", charging);
        cJSON_AddItemToObject(root, "battery", battery);
    }

    // Network
    auto network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "type", "cellular");
    // cJSON_AddStringToObject(network, "carrier", modem_->GetCarrierName().c_str());
    cJSON_AddStringToObject(network, "signal", "strong");
    cJSON_AddItemToObject(root, "network", network);

    auto json_str = cJSON_PrintUnformatted(root);
    std::string json(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return json;
}
