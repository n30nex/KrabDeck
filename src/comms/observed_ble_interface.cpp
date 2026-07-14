// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "observed_ble_interface.h"

#if defined(ESP32_PLATFORM) && defined(SIGURDOS_COMPANION_BLE) && SIGURDOS_COMPANION_BLE

namespace sigurdos {
namespace comms {

void ObservedSerialBLEInterface::refreshConnectionState()
{
    _stats.enabled = SerialBLEInterface::isEnabled();
    _stats.connected = SerialBLEInterface::isConnected();
    _stats.advertising_expected = _stats.enabled && !_stats.connected;
}

void ObservedSerialBLEInterface::begin(const char* prefix, char* name, uint32_t pin_code)
{
    BleTaskMutex::Guard guard(_state_mutex);
    SerialBLEInterface::begin(prefix, name, pin_code);
    _rx_queue.clear();
    _stats = BleSerialObserverStats{};
    _stats.begun = true;
    _stats.begin_count = 1;
    refreshConnectionState();
}

void ObservedSerialBLEInterface::enable()
{
    BleTaskMutex::Guard guard(_state_mutex);
    SerialBLEInterface::enable();  // also clears the (now unused) base buffers
    _rx_queue.clear();
    _stats.enable_count++;
    refreshConnectionState();
}

void ObservedSerialBLEInterface::disable()
{
    BleTaskMutex::Guard guard(_state_mutex);
    SerialBLEInterface::disable();
    _rx_queue.clear();
    _stats.disable_count++;
    refreshConnectionState();
}

bool ObservedSerialBLEInterface::isEnabled() const
{
    BleTaskMutex::Guard guard(_state_mutex);
    return SerialBLEInterface::isEnabled();
}

bool ObservedSerialBLEInterface::isConnected() const
{
    BleTaskMutex::Guard guard(_state_mutex);
    return SerialBLEInterface::isConnected();
}

bool ObservedSerialBLEInterface::isWriteBusy() const
{
    BleTaskMutex::Guard guard(_state_mutex);
    return SerialBLEInterface::isWriteBusy();
}

BleSerialObserverStats ObservedSerialBLEInterface::stats() const
{
    BleTaskMutex::Guard guard(_state_mutex);
    return _stats;
}

size_t ObservedSerialBLEInterface::writeFrame(const uint8_t src[], size_t len)
{
    BleTaskMutex::Guard guard(_state_mutex);
    size_t written = SerialBLEInterface::writeFrame(src, len);
    if (written > 0) {
        _stats.tx_frame_count++;
        _stats.last_tx_code = src ? src[0] : 0;
    } else if (len > 0) {
        _stats.tx_drop_count++;
    }
    refreshConnectionState();
    return written;
}

size_t ObservedSerialBLEInterface::checkRecvFrame(uint8_t dest[])
{
    BleTaskMutex::Guard guard(_state_mutex);
    // Drives the base transmit queue and connection housekeeping. The base
    // receive queue stays empty (onWrite no longer feeds it), so any frame
    // returned here comes from _rx_queue.
    size_t len = SerialBLEInterface::checkRecvFrame(dest);
    if (len == 0) {
        len = _rx_queue.pop(dest);
    }
    if (len > 0) {
        _stats.rx_frame_count++;
        _stats.last_rx_code = dest ? dest[0] : 0;
    }
    refreshConnectionState();
    return len;
}

uint32_t ObservedSerialBLEInterface::onPassKeyRequest()
{
    BleTaskMutex::Guard guard(_state_mutex);
    return SerialBLEInterface::onPassKeyRequest();
}

void ObservedSerialBLEInterface::onPassKeyNotify(uint32_t pass_key)
{
    BleTaskMutex::Guard guard(_state_mutex);
    SerialBLEInterface::onPassKeyNotify(pass_key);
}

bool ObservedSerialBLEInterface::onConfirmPIN(uint32_t pass_key)
{
    BleTaskMutex::Guard guard(_state_mutex);
    return SerialBLEInterface::onConfirmPIN(pass_key);
}

bool ObservedSerialBLEInterface::onSecurityRequest()
{
    BleTaskMutex::Guard guard(_state_mutex);
    return SerialBLEInterface::onSecurityRequest();
}

void ObservedSerialBLEInterface::onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl)
{
    BleTaskMutex::Guard guard(_state_mutex);
    if (cmpl.success) {
        _stats.auth_success_count++;
    } else {
        _stats.auth_failure_count++;
    }
    SerialBLEInterface::onAuthenticationComplete(cmpl);
    refreshConnectionState();
}

void ObservedSerialBLEInterface::onConnect(BLEServer* server)
{
    BleTaskMutex::Guard guard(_state_mutex);
    SerialBLEInterface::onConnect(server);
    refreshConnectionState();
}

void ObservedSerialBLEInterface::onConnect(BLEServer* server,
                                           esp_ble_gatts_cb_param_t* param)
{
    BleTaskMutex::Guard guard(_state_mutex);
    _stats.connect_count++;
    if (param) {
        _stats.last_conn_id = param->connect.conn_id;
    }
    SerialBLEInterface::onConnect(server, param);
    refreshConnectionState();
}

void ObservedSerialBLEInterface::onMtuChanged(BLEServer* server,
                                              esp_ble_gatts_cb_param_t* param)
{
    BleTaskMutex::Guard guard(_state_mutex);
    _stats.mtu_change_count++;
    if (server && param) {
        _stats.last_conn_id = param->mtu.conn_id;
        _stats.last_mtu = server->getPeerMTU(param->mtu.conn_id);
    }
    SerialBLEInterface::onMtuChanged(server, param);
    refreshConnectionState();
}

void ObservedSerialBLEInterface::onDisconnect(BLEServer* server)
{
    BleTaskMutex::Guard guard(_state_mutex);
    _stats.disconnect_count++;
    SerialBLEInterface::onDisconnect(server);
    // Pending frames belong to the dead connection; the base class drops its
    // own buffers on the disconnect transition, mirror that for _rx_queue.
    _rx_queue.clear();
    refreshConnectionState();
}

void ObservedSerialBLEInterface::onWrite(BLECharacteristic* characteristic,
                                         esp_ble_gatts_cb_param_t* param)
{
    BleTaskMutex::Guard guard(_state_mutex);
    (void)param;
    // NET-002 (#813): deliberately NOT forwarded to the base class. Received
    // frames stay in the synchronized handoff queue while the adapter task
    // mutex makes the surrounding callback and connection state coherent.
    if (!characteristic) return;
    const size_t len = characteristic->getLength();
    if (len == 0) return;
    _stats.ble_write_count++;
    if (!_rx_queue.push(characteristic->getData(), len)) {
        _stats.ble_write_drop_count++;  // oversize frame or queue full
    }
    refreshConnectionState();
}

} // namespace comms
} // namespace sigurdos

#endif
