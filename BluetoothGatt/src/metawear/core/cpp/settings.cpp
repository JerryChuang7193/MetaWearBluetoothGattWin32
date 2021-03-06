#include "metawear/core/module.h"

#include "metawear/core/settings.h"
#include "settings_private.h"

#include "constant.h"
#include "datasignal_private.h"
#include "event_private.h"
#include "metawearboard_def.h"
#include "metawearboard_macro.h"
#include "register.h"
#include "responseheader.h"
#include "settings_register.h"

#include <cstring>
#include <vector>

using std::memcpy;
using std::stringstream;
using std::vector;

const float AD_INTERVAL_STEP= 0.625f, CONN_INTERVAL_STEP= 1.25f, TIMEOUT_STEP= 10;
const uint8_t CONN_PARAMS_REVISION= 1, DISCONNECTED_EVENT_REVISION= 2, BATTERY_REVISION= 3, WHITELIST_REVISION = 6;

const ResponseHeader 
    SETTINGS_BATTERY_STATE_RESPONSE_HEADER(MBL_MW_MODULE_SETTINGS, READ_REGISTER(ORDINAL(SettingsRegister::BATTERY_STATE))),
    SETTINGS_DISCONNECT_EVENT_RESPONSE_HEADER(MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::DISCONNECT_EVENT)),
    SETTINGS_MAC_RESPONSE_HEADER(MBL_MW_MODULE_SETTINGS, READ_REGISTER(ORDINAL(SettingsRegister::MAC)));

void init_settings_module(MblMwMetaWearBoard *board) {
    if (board->module_info.at(MBL_MW_MODULE_SETTINGS).revision >= BATTERY_REVISION) {
        MblMwDataSignal* battery;
        if (board->module_events.count(SETTINGS_BATTERY_STATE_RESPONSE_HEADER)) {
            battery = dynamic_cast<MblMwDataSignal*>(board->module_events[SETTINGS_BATTERY_STATE_RESPONSE_HEADER]);
        } else {
            battery = new MblMwDataSignal(SETTINGS_BATTERY_STATE_RESPONSE_HEADER, board, DataInterpreter::SETTINGS_BATTERY_STATE, 1, 3, 0, 0);
            board->module_events[SETTINGS_BATTERY_STATE_RESPONSE_HEADER] = battery;
        }
        if (!battery->components.size()) {
            // MBL_MW_MODULE_SETTINGS_BATT_VOLTAGE_INDEX
            battery->components.push_back(new MblMwDataSignal(SETTINGS_BATTERY_STATE_RESPONSE_HEADER, board, DataInterpreter::UINT32, 1, 2, 0, 1));
            // MBL_MW_MODULE_SETTINGS_BATT_CHARGE_INDEX
            battery->components.push_back(new MblMwDataSignal(SETTINGS_BATTERY_STATE_RESPONSE_HEADER, board, DataInterpreter::UINT32, 1, 1, 0, 0));
        }
        board->responses[SETTINGS_BATTERY_STATE_RESPONSE_HEADER] = response_handler_data_no_id;
    }
    if (board->module_info.at(MBL_MW_MODULE_SETTINGS).revision >= DISCONNECTED_EVENT_REVISION) {
        if (!board->module_events.count(SETTINGS_DISCONNECT_EVENT_RESPONSE_HEADER)) {
            board->module_events[SETTINGS_DISCONNECT_EVENT_RESPONSE_HEADER] = new MblMwEvent(SETTINGS_DISCONNECT_EVENT_RESPONSE_HEADER, board);
        }
        if (!board->module_events.count(SETTINGS_MAC_RESPONSE_HEADER)) {
            board->module_events[SETTINGS_MAC_RESPONSE_HEADER] = new MblMwDataSignal(SETTINGS_MAC_RESPONSE_HEADER, board, 
                DataInterpreter::MAC_ADDRESS, FirmwareConverter::DEFAULT, 1, 6, 0, 0);
        }
        board->responses[SETTINGS_MAC_RESPONSE_HEADER]= response_handler_data_no_id;   
    }
}

MblMwEvent* mbl_mw_settings_get_disconnect_event(const MblMwMetaWearBoard *board) {
    GET_EVENT(SETTINGS_DISCONNECT_EVENT_RESPONSE_HEADER);
}

MblMwDataSignal* mbl_mw_settings_get_battery_state_data_signal(const MblMwMetaWearBoard *board) {
    GET_DATA_SIGNAL(SETTINGS_BATTERY_STATE_RESPONSE_HEADER);
}

METAWEAR_API MblMwDataSignal* mbl_mw_settings_get_mac_data_signal(const MblMwMetaWearBoard *board) {
    GET_DATA_SIGNAL(SETTINGS_MAC_RESPONSE_HEADER);
}

void mbl_mw_settings_set_device_name(const MblMwMetaWearBoard *board, const uint8_t *device_name, uint8_t len) {
    vector<uint8_t> command(device_name, device_name + len);
    command.insert(command.begin(), {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::DEVICE_NAME)});
    send_command(board, command.data(), (uint8_t) command.size());
}

void mbl_mw_settings_set_ad_interval(const MblMwMetaWearBoard *board, uint16_t interval, uint8_t timeout) {
    vector<uint8_t> command = {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::AD_INTERVAL), 0, 0, timeout};

    if (board->module_info.at(MBL_MW_MODULE_SETTINGS).revision >= CONN_PARAMS_REVISION) {
        interval/= AD_INTERVAL_STEP;
    }
    memcpy(command.data() + 2, &interval, sizeof(interval));

    if (board->module_info.at(MBL_MW_MODULE_SETTINGS).revision >= WHITELIST_REVISION) {
        command.push_back(0);
    }

    send_command(board, command.data(), (uint8_t) command.size());
}

void mbl_mw_settings_set_tx_power(const MblMwMetaWearBoard *board, int8_t tx_power) {
    uint8_t command[3]= {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::TX_POWER), static_cast<uint8_t>(tx_power)};
    SEND_COMMAND;
}

void mbl_mw_settings_start_advertising(const MblMwMetaWearBoard *board) {
    uint8_t command[2]= {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::START_ADVERTISING)};
    SEND_COMMAND;
}

void mbl_mw_settings_set_scan_response(const MblMwMetaWearBoard *board, const uint8_t *response, uint8_t len) {
    if (len > MW_CMD_MAX_LENGTH) {
        vector<uint8_t> first(response, response + 13), second(response + 13, response + len);

        first.insert(first.begin(), {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::PARTIAL_SCAN_RESPONSE)});
        second.insert(second.begin(), {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::SCAN_RESPONSE)});

        send_command(board, first.data(), (uint8_t) first.size());
        send_command(board, second.data(), (uint8_t) second.size());
    } else {
        vector<uint8_t> command(response, response + len);

        command.insert(command.begin(), {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::SCAN_RESPONSE)});
        send_command(board, command.data(), (uint8_t) command.size());
    }
}

void mbl_mw_settings_set_connection_parameters(const MblMwMetaWearBoard *board, float min_conn_interval, float max_conn_interval, uint16_t latency, 
        uint16_t timeout) {
    uint8_t command[10]= {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::CONNECTION_PARAMS)};
    uint16_t parameters[4]= {static_cast<uint16_t>(min_conn_interval / CONN_INTERVAL_STEP), 
            static_cast<uint16_t>(max_conn_interval / CONN_INTERVAL_STEP), latency, static_cast<uint16_t>(timeout / TIMEOUT_STEP)};

    memcpy(command + 2, parameters, sizeof(parameters));
    SEND_COMMAND;
}

void mbl_mw_settings_add_whitelist_address(const MblMwMetaWearBoard *board, uint8_t index, const MblMwBtleAddress *address) {
    if (board->module_info.at(MBL_MW_MODULE_SETTINGS).revision >= WHITELIST_REVISION) {
        uint8_t command[10]= {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::WHITELIST_ADDRESSES), index};
        memcpy(command + 3, address, sizeof(MblMwBtleAddress));
        SEND_COMMAND;
    }
}

void mbl_mw_settings_set_whitelist_filter_mode(const MblMwMetaWearBoard *board, MblMwWhitelistFilter mode) {
    if (board->module_info.at(MBL_MW_MODULE_SETTINGS).revision >= WHITELIST_REVISION) {
        uint8_t command[3]= {MBL_MW_MODULE_SETTINGS, ORDINAL(SettingsRegister::WHITELIST_FILTER_MODE), static_cast<uint8_t>(mode)};
        SEND_COMMAND;
    }
}

void create_settings_uri(const MblMwDataSignal* signal, stringstream& uri) {
    switch(CLEAR_READ(signal->header.register_id)) {
    case ORDINAL(SettingsRegister::BATTERY_STATE):
        uri << "battery";
        switch(signal->length()) {
        case 1:
            uri << "[0]";
            break;
        case 2:
            uri << "[1]";
            break;
        }
        break;
    }
}
