#include "cn105.h"
#include <algorithm>  // for std::max

using namespace esphome;

uint8_t CN105Climate::checkSum(uint8_t bytes[], int len) {
    return cn105_protocol::checksum(bytes, len);
}


void CN105Climate::sendFirstConnectionPacket() {
    if (this->isUARTReady_()) {
        this->lastReconnectTimeMs = CUSTOM_MILLIS;          // marker to prevent to many reconnections
        this->setHeatpumpConnected(false);
        uint8_t packet[CONNECT_LEN];
        memcpy(packet, CONNECT, CONNECT_LEN);
        packet[3] = this->profile_->id();
        packet[CONNECT_LEN - 1] = checkSum(packet, CONNECT_LEN - 1);

        ESP_LOGI(LOG_CONN_TAG, "Sending the connection packet in Standard mode (0x%02X), profile 0x%02X...", packet[1], packet[3]);
        this->hpPacketDebug(packet, CONNECT_LEN, LOG_CONN_TAG);

        this->writePacket(packet, CONNECT_LEN, false);      // checkIsActive=false because it's the first packet and we don't have any reply yet

        this->lastSend = CUSTOM_MILLIS;
        this->lastConnectRqTimeMs = CUSTOM_MILLIS;
        this->nbHeatpumpConnections_++;

        // Arms the 0x7A reply timeout (reconnects if the heatpump stays silent) and
        // drops any stale installer handshake state from a previous attempt.
        this->handshake_.on_connect_sent();

    } else {
        ESP_LOGE(LOG_CONN_TAG, "UART doesn't seem to be connected...");
        this->setupUART();
        // this delay to prevent a logging flood should never happen
        CUSTOM_DELAY(750);
    }
}

void CN105Climate::sendInstallerConnectPacket() {
    uint8_t packet[INST_CONNECT_LEN];
    memcpy(packet, INST_CONNECT, INST_CONNECT_LEN);
    packet[3] = this->profile_->id();
    packet[INST_CONNECT_LEN - 1] = checkSum(packet, INST_CONNECT_LEN - 1);

    ESP_LOGI(LOG_CONN_TAG, "Standard session established, upgrading with installer handshake (0x%02X), profile 0x%02X...", packet[1], packet[3]);
    this->hpPacketDebug(packet, INST_CONNECT_LEN, LOG_CONN_TAG);

    this->writePacket(packet, INST_CONNECT_LEN, false);
    this->lastSend = CUSTOM_MILLIS;
}


// void CN105Climate::statusChanged() {
//     ESP_LOGD(TAG, "hpStatusChanged ->");
//     this->current_temperature = currentStatus.roomTemperature;

//     ESP_LOGD(TAG, "t°: %f", currentStatus.roomTemperature);
//     ESP_LOGD(TAG, "operating: %d", currentStatus.operating);
//     ESP_LOGD(TAG, "compressor freq: %f", currentStatus.compressorFrequency);

//     this->updateAction();
//     this->publish_state();
// }

void CN105Climate::prepareInfoPacket(uint8_t* packet, int length) {
    ESP_LOGV(TAG, "preparing info packet...");

    memset(packet, 0, length * sizeof(uint8_t));

    for (int i = 0; i < INFOHEADER_LEN && i < length; i++) {
        packet[i] = INFOHEADER[i];
    }
    if (length > 3) {
        packet[3] = this->profile_->id();
    }
}

void CN105Climate::prepareSetPacket(uint8_t* packet, int length) {
    ESP_LOGV(TAG, "preparing Set packet...");
    memset(packet, 0, length * sizeof(uint8_t));

    for (int i = 0; i < HEADER_LEN && i < length; i++) {
        packet[i] = HEADER[i];
    }
    if (length > 3) {
        packet[3] = this->profile_->id();
    }
}

bool CN105Climate::writePacket(uint8_t* packet, int length, bool checkIsActive) {

    if ((this->isUARTReady_()) &&
        (this->isHeatpumpConnectionActive() || (!checkIsActive))) {

        ESP_LOGD(TAG, "writing packet...");
        this->hpPacketDebug(packet, length, "WRITE");

        for (int i = 0; i < length; i++) {
            this->get_hw_serial_()->write_byte((uint8_t)packet[i]);
        }

        // Prevent sending wantedSettings too soon after writing for example the remote temperature update packet
        this->lastSend = CUSTOM_MILLIS;
        return true;
    }

    // Nothing is saved. disconnectUART() releases the bus claim and the reconnect starts a fresh
    // handshake. The caller keeps its own wanted state queued; it is re-sent from loop() once
    // the connection is fully established (isHeatpumpConnected()).
    ESP_LOGW(TAG, "could not write as asked, because UART is not connected: reconnecting");
    this->reconnectUART();
    return false;
}

const char* CN105Climate::getModeSetting() {
    if (this->wantedSettings.mode) {
        return this->wantedSettings.mode;
    } else {
        return this->currentSettings.mode;
    }
}

const char* CN105Climate::getPowerSetting() {
    if (this->wantedSettings.power) {
        return this->wantedSettings.power;
    } else {
        return this->currentSettings.power;
    }
}

const char* CN105Climate::getVaneSetting() {
    if (this->wantedSettings.vane) {
        return this->wantedSettings.vane;
    } else {
        return this->currentSettings.vane;
    }
}

const char* CN105Climate::getWideVaneSetting() {
    return this->profile_->wide_vane_setting();
}

const char* CN105Climate::getFanSpeedSetting() {
    if (this->wantedSettings.fan) {
        return this->wantedSettings.fan;
    } else {
        return this->currentSettings.fan;
    }
}

float CN105Climate::getTemperatureSetting() {
    if (this->wantedSettings.temperature != -1.0) {
        return this->wantedSettings.temperature;
    } else {
        return this->currentSettings.temperature;
    }
}
const char* CN105Climate::getAirflowControlSetting() {
    if (this->wantedRunStates.airflow_control) {
        return this->wantedRunStates.airflow_control;
    } else {
        return this->currentRunStates.airflow_control;
    }
}
bool CN105Climate::getAirPurifierRunState() {
    if (this->wantedRunStates.air_purifier != this->currentRunStates.air_purifier) {
        return this->wantedRunStates.air_purifier;
    } else {
        return this->currentRunStates.air_purifier;
    }
}
bool CN105Climate::getNightModeRunState() {
    if (this->wantedRunStates.night_mode != this->currentRunStates.night_mode) {
        return this->wantedRunStates.night_mode;
    } else {
        return this->currentRunStates.night_mode;
    }
}
bool CN105Climate::getCirculatorRunState() {
    if (this->wantedRunStates.circulator != this->currentRunStates.circulator) {
        return this->wantedRunStates.circulator;
    } else {
        return this->currentRunStates.circulator;
    }
}


void CN105Climate::createPacket(uint8_t* packet) {
    prepareSetPacket(packet, PACKET_LEN);

    this->profile_->encode_control(packet);
    packet[21] = checkSum(packet, 21);
}

void CN105Climate::publishWantedSettingsStateToHA() {
    this->profile_->apply_wanted_settings();
    this->publish_state();
}

void CN105Climate::publishWantedRunStatesStateToHA() {
    this->profile_->apply_wanted_run_states();
}

void CN105Climate::sendWantedSettingsDelegate() {
    // Too old to still reflect what the user wants (link was down for a while): drop it.
    // The next poll resyncs the UI because currentSettings is reset on every new session.
    if (cn105_protocol::is_wanted_settings_expired(static_cast<uint32_t>(this->wantedSettings.lastChange),
                                                   CUSTOM_MILLIS, WANTED_SETTINGS_MAX_AGE_MS)) {
        ESP_LOGW(TAG, "Dropping wantedSettings queued for more than %u ms", (unsigned) WANTED_SETTINGS_MAX_AGE_MS);
        this->wantedSettings.resetSettings();
        this->bus_gate_.release();     // sendWantedSettings() claimed it before calling us
        this->publish_state();
        return;
    }

    this->lastSend = CUSTOM_MILLIS;
    ESP_LOGI(TAG, "sending wantedSettings...");
    this->debugSettings("wantedSettings", wantedSettings);
    // and then we send the update packet
    uint8_t packet[PACKET_LEN] = {};
    this->createPacket(packet);

    if (!this->writePacket(packet, PACKET_LEN)) {
        // hasChanged stays true, hasBeenSent stays false: checkPendingWantedSettings() sends the
        // newest wanted state once the connection is back. Nothing was published optimistically.
        ESP_LOGW(TAG, "wantedSettings not sent, keeping them queued");
        return;
    }

    this->wantedSettings.hasBeenSent = true;
    this->hpPacketDebug(packet, 22, "WRITE_SETTINGS");

    this->publishWantedSettingsStateToHA();

    // as soon as the packet is sent, we reset the settings
    this->wantedSettings.resetSettings();

    // as we've just sent a packet to the heatpump, we let it time for process
    // this might not be necessary but, we give it a try because of issue #32
    // https://github.com/echavet/MitsubishiCN105ESPHome/issues/32
    this->loopCycle.deferCycle();
}

/**
 * builds and send all an update packet to the heatpump
 *
 *
*/
void CN105Climate::sendWantedSettings() {
    if (this->isHeatpumpConnectionActive() && this->isUARTReady_()) {
        if (CUSTOM_MILLIS - this->lastSend > 300) {        // we don't want to send too many packets

            //this->cycleEnded();   // only if we let the cycle be interrupted to send wented settings

            if (!this->bus_gate_.begin(cn105::BusRequestKind::SET_SETTINGS, cn105::BusExpectedResponse::ACK, CUSTOM_MILLIS)) {
                ESP_LOGD(TAG, "will sendWantedSettings later because another request is waiting for its reply...");
                return;
            }

#ifdef USE_ESP32
            std::lock_guard<std::mutex> guard(wantedSettingsMutex);
            this->sendWantedSettingsDelegate();
#else
            this->emulateMutex("WRITE_SETTINGS", std::bind(&CN105Climate::sendWantedSettingsDelegate, this));

#endif

        } else {
            ESP_LOGD(TAG, "will sendWantedSettings later because we've sent one too recently...");
        }
    } else {
        this->reconnectIfConnectionLost();
    }
}

void CN105Climate::buildAndSendRequestPacket(int packetType) {
    // Legacy path kept temporarily if some callsites still pass packetType indices.
    // Map legacy indices to real codes and delegate to buildAndSendInfoPacket.
    uint8_t code = 0x02; // default to settings
    switch (packetType) {
    case 0: code = 0x02; break; // RQST_PKT_SETTINGS
    case 1: code = 0x03; break; // RQST_PKT_ROOM_TEMP
    case 2: code = 0x04; break; // RQST_PKT_UNKNOWN
    case 3: code = 0x05; break; // RQST_PKT_TIMERS
    case 4: code = 0x06; break; // RQST_PKT_STATUS
    case 5: code = 0x09; break; // RQST_PKT_STANDBY
    case 6: code = 0x42; break; // RQST_PKT_HVAC_OPTIONS
    default: code = 0x02; break;
    }
    this->buildAndSendInfoPacket(code);
}

bool CN105Climate::buildAndSendInfoPacket(uint8_t code) {
    // A soft-timeout can fire after a reconnect and continue a dead cycle: never poll
    // before the handshake has completed.
    if (!this->isHeatpumpConnected()) {
        return false;
    }
    if (!this->bus_gate_.begin(cn105::BusRequestKind::INFO_REQUEST, cn105::BusExpectedResponse::DATA,
                               CUSTOM_MILLIS, cn105::BusGate::kReplyTimeoutMs, code)) {
        ESP_LOGW(TAG, "Not sending info request 0x%02X: another request is waiting for its reply", code);
        return false;
    }
    // Only once the read is really going out: a refused claim must not leave a snapshot behind.
    if (code == FUNCTIONS_GET_PART1) {
        this->captureStateBeforeFunctions();
    }
    uint8_t packet[PACKET_LEN] = {};
    createInfoPacket(packet, code);
    // false -> lambda ends the cycle
    return this->writePacket(packet, PACKET_LEN);
}

void CN105Climate::buildAndSendRequestsInfoPackets() {
    if (this->isHeatpumpConnected()) {
        ESP_LOGV(LOG_UPD_INT_TAG, "triggering infopacket because of update interval tick");
        ESP_LOGV("CONTROL_WANTED_SETTINGS", "hasChanged is %s", wantedSettings.hasChanged ? "true" : "false");
        this->loopCycle.cycleStarted();
        this->nbCycles_++;
        // Envoie la première requête activable (la liste est enregistrée une fois au constructeur)
        this->scheduler_.send_next_after(0x00); // 0x00 -> start, pick first eligible
    } else {
        this->reconnectIfConnectionLost();
    }
}

void CN105Climate::createInfoPacket(uint8_t* packet, uint8_t code) {
    ESP_LOGD(TAG, "creating Info packet");
    prepareInfoPacket(packet, PACKET_LEN);

    // directly set requested info code (0x02, 0x03, 0x06, 0x09, 0x42, ...)
    packet[5] = code;

    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
}


void CN105Climate::sendRemoteTemperaturePacket() {
    if (!this->profile_->allow_operation(cn105::ProfileFeature::REMOTE_TEMPERATURE)) return;

    // Build and send the remote temperature packet (0x07) without affecting watchdog/keep-alive timers

    // Debounce logic: avoid flooding the bus with identical temperature values
    // Only skip if: same temperature AND sent recently (within half of keep-alive interval, min 5s)
    uint32_t now = CUSTOM_MILLIS;
    uint32_t min_interval = this->remote_temp_keepalive_interval_ms_ > 0
        ? std::max(this->remote_temp_keepalive_interval_ms_ / 2, (uint32_t)5000)
        : 5000;  // Default 5s if keep-alive disabled

    bool temp_changed = (this->remoteTemperature_ != this->last_remote_temp_sent_);
    uint32_t elapsed = now - this->last_remote_temp_send_ms_;

    if (!temp_changed && elapsed < min_interval) {
        // Debounce: skip this send
        this->remote_temp_debounce_skip_count_++;

        // Detect conflicting heartbeat pattern: multiple rapid calls with same value
        // After 3 consecutive skips, warn the user (only once)
        // Only show warning if keep-alive is enabled - if disabled, manual heartbeat is intentional
        if (this->remote_temp_debounce_skip_count_ >= 3 &&
            !this->remote_temp_heartbeat_warning_shown_ &&
            this->remote_temp_keepalive_interval_ms_ > 0) {
            this->remote_temp_heartbeat_warning_shown_ = true;
            ESP_LOGW(LOG_REMOTE_TEMP,
                "Detected repeated remote temperature calls with unchanged value (%.1f). "
                "If you have a manual heartbeat/interval in YAML, consider removing it - "
                "the built-in keep-alive (every %lu ms) handles this automatically. "
                "See 'remote_temperature_keepalive_interval' option in documentation.",
                this->remoteTemperature_, (unsigned long)this->remote_temp_keepalive_interval_ms_);
        }

        ESP_LOGD(LOG_REMOTE_TEMP, "Debounce: skipping remote temp send (same value %.1f, %lu ms since last send, min interval %lu ms, skip #%d)",
            this->remoteTemperature_, (unsigned long)elapsed, (unsigned long)min_interval, this->remote_temp_debounce_skip_count_);
        return;
    }

    if (!this->bus_gate_.begin(cn105::BusRequestKind::SET_REMOTE_TEMPERATURE, cn105::BusExpectedResponse::ACK, now)) {
        ESP_LOGW(LOG_REMOTE_TEMP, "Not sending remote temperature: another request is waiting for its reply");
        shouldSendExternalTemperature_ = true;
        return;
    }

    // Reset debounce skip counter on successful send
    this->remote_temp_debounce_skip_count_ = 0;

    uint8_t packet[PACKET_LEN] = {};

    prepareSetPacket(packet, PACKET_LEN);

    this->profile_->encode_remote_temperature(packet);
    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;

    ESP_LOGD(LOG_REMOTE_TEMP, "Sending remote temperature packet... -> %.1f%s",
        this->remoteTemperature_, temp_changed ? " (changed)" : " (keep-alive)");
    if (!writePacket(packet, PACKET_LEN)) {
        ESP_LOGW(LOG_REMOTE_TEMP, "Not sending remote temperature, keeping them queued");
        shouldSendExternalTemperature_ = true;
        return;
    }

    // Update debounce tracking
    this->last_remote_temp_send_ms_ = now;
    this->last_remote_temp_sent_ = this->remoteTemperature_;
}

void CN105Climate::sendRemoteTemperature() {
    // The flag stays set while another request is waiting, so the send is retried.
    if (!this->bus_gate_.is_idle(CUSTOM_MILLIS)) {
        return;
    }
    this->shouldSendExternalTemperature_ = false;

    // Send the packet
    this->sendRemoteTemperaturePacket();
}

void CN105Climate::sendWantedRunStates() {
    if (!this->profile_->allow_operation(cn105::ProfileFeature::AUXILIARY_CONTROLS)) {
        this->wantedRunStates.resetSettings();
        return;
    }

    if (!this->bus_gate_.begin(cn105::BusRequestKind::SET_RUN_STATES, cn105::BusExpectedResponse::ACK, CUSTOM_MILLIS)) {
        ESP_LOGD(LOG_SET_RUN_STATE, "Will send run states later: another request is waiting for its reply");
        return;
    }

    uint8_t packet[PACKET_LEN] = {};

    prepareSetPacket(packet, PACKET_LEN);

    this->profile_->encode_run_states(packet);
    // Add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
    ESP_LOGD(LOG_SET_RUN_STATE, "Sending set run state package (0x08)");
    if (!writePacket(packet, PACKET_LEN)) {
        ESP_LOGW(LOG_SET_RUN_STATE, "Run states not sent, keeping them queued");
        // hasChanged stays true -> retried by loop()
        return;
    }

    this->publishWantedRunStatesStateToHA();
    this->wantedRunStates.resetSettings();
    this->loopCycle.deferCycle();
}

void CN105Climate::handleHeatpumpAck() {
    const std::optional<cn105::BusRequestKind> completed_kind = this->bus_gate_.complete(cn105::BusResponse::ACK, CUSTOM_MILLIS);
    if (!completed_kind.has_value()) {
        ESP_LOGD(LOG_ACK, "Ignoring ack: no request is waiting for it");
        return;
    }
    switch (*completed_kind) {
    case cn105::BusRequestKind::SET_FUNCTIONS_PART1:
        this->sendSecondFunctionsPacket();
        break;
    case cn105::BusRequestKind::SET_FUNCTIONS_PART2:
        ESP_LOGD(TAG, "setFunctions complete");
        this->isGetFunctions_ = true;
        break;
    default:
        this->updateSuccess();
        break;
    }
}

void CN105Climate::sendSecondFunctionsPacket() {
    if (!this->bus_gate_.begin(cn105::BusRequestKind::SET_FUNCTIONS_PART2, cn105::BusExpectedResponse::ACK, CUSTOM_MILLIS)) {
        ESP_LOGW(TAG, "setFunctions part 2 not sent: another request is waiting for its reply, re-reading functions");
        this->isGetFunctions_ = true;
        return;
    }
    ESP_LOGD(TAG, "sending a setFunctions packet part 2");
    if (!this->writePacket(this->functions_second_packet_, PACKET_LEN)) {
        // Part 1 was already acknowledged: the unit may hold a half-applied write. Re-read it.
        this->isGetFunctions_ = true;
    }
}
