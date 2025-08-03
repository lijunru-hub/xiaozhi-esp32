#ifndef NT26_BOARD_H
#define NT26_BOARD_H

#include <memory>
#include "board.h"
#include "iot_uart_eth.h"
#include "iot_eth.h"

class Nt26Board : public Board {
protected:
    iot_uart_eth_config_t uart_eth_config_;
    iot_eth_driver_t *uart_eth_handle_ = nullptr;
    iot_eth_handle_t eth_handle_ = nullptr;
    virtual std::string GetBoardJson() override;

public:
    Nt26Board(iot_uart_eth_config_t uart_eth_config);
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual NetworkInterface* GetNetwork() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveMode(bool enabled) override;
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
    virtual std::string GetDeviceStatusJson() override;
};

#endif // NT26_BOARD_H
