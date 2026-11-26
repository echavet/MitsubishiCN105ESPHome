#pragma once
/// functions_restore.h — Save and restore the unit state around a Functions (ISU) access.
///
/// Reading or writing Function Settings can make some units power themselves off (#784).
/// FunctionsSnapshot keeps the state saved before the access and decides, from each
/// settings reply (0x02), whether the unit was switched off and what to re-send.
///
/// Two phases, each with its own time limit:
///   1. read in progress (after capture(), before open_window()): FUNCTIONS_SNAPSHOT_MAX_AGE_MS
///   2. window open (after the 0x22 reply):                        window_ms (default FUNCTIONS_RESTORE_WINDOW_MS)
///
/// Nothing here depends on ESPHome. Time is passed in and unsigned subtraction keeps
/// every age correct across millis() rollover. CN105Climate stores the snapshot,
/// supplies the clock and applies the returned Restore to its wanted settings.

#include <cmath>
#include <cstdint>
#include <cstring>
#include "cn105_protocol.h"
#include "cn105_types.h"

namespace esphome::cn105 {

class FunctionsSnapshot {
public:
    /// What to queue after a power-off was detected. A null pointer means "do not send".
    struct Restore {
        bool restore = false;
        const char* power = nullptr;
        const char* mode = nullptr;
        const char* fan = nullptr;
        const char* vane = nullptr;
        const char* wide_vane = nullptr;
        bool set_temperature = false;
        float temperature = -1.0f;  // raw saved setpoint: the caller quantizes it for the unit
    };

    void set_enabled(bool enabled) { this->enabled_ = enabled; }
    bool enabled() const { return this->enabled_; }
    bool valid() const { return this->valid_; }
    bool window_open() const { return this->window_open_; }
    const heatpumpSettings& saved() const { return this->saved_; }

    /// Phase 1 is bounded by the snapshot max age, phase 2 by the restore window.
    bool expired(uint32_t now_ms) const {
        return cn105_protocol::is_wanted_settings_expired(
            this->saved_ms_, now_ms,
            this->window_open_ ? this->window_ms_ : FUNCTIONS_SNAPSHOT_MAX_AGE_MS);
    }

    void discard() {
        this->valid_ = false;
        this->window_open_ = false;
    }

    /// Save the state before a functions access (read part 1 or write).
    /// @return true when a new snapshot was saved
    bool capture(const heatpumpSettings& current, uint32_t now_ms) {
        if (!this->enabled_) {
            return false;
        }
        if (this->valid_ && !this->expired(now_ms)) {
            // Same sequence (e.g. SET followed by GET): keep the state saved before its first step.
            this->window_open_ = false;
            this->saved_ms_ = now_ms;
            return false;
        }
        this->discard();
        if (current.power == nullptr || current.mode == nullptr) {
            return false;
        }
        this->saved_ = current;
        this->valid_ = true;
        this->saved_ms_ = now_ms;
        return true;
    }

    /// The read finished (0x22 arrived): from now on watch for a power-off.
    /// @param window_ms  how long to watch; callers pass at least one poll interval on top of the base window
    void open_window(uint32_t now_ms, uint32_t window_ms = FUNCTIONS_RESTORE_WINDOW_MS) {
        if (!this->valid_) {
            return;
        }
        this->window_ms_ = window_ms;
        this->window_open_ = true;
        this->saved_ms_ = now_ms;  // phase 2 starts now
    }

    /// Evaluate a settings reply, before it is applied.
    /// @param user_change_pending  wantedSettings.hasChanged: someone else owns the state
    Restore on_settings(const heatpumpSettings& received, bool user_change_pending, uint32_t now_ms) {
        Restore result;
        if (!this->valid_) {
            return result;
        }
        if (this->expired(now_ms)) {
            this->discard();  // read never finished, or the restore window is over
            return result;
        }
        if (!this->window_open_) {
            return result;  // read still in progress
        }
        if (user_change_pending) {
            this->discard();
            return result;
        }
        const heatpumpSettings snapshot = this->saved_;
        // capture() guarantees non-null power and mode
        if (std::strcmp(snapshot.power, "ON") != 0) {
            this->discard();  // it was off: nothing to restore
            return result;
        }
        if (received.power == nullptr || std::strcmp(received.power, snapshot.power) == 0) {
            return result;  // still on: keep watching until the window ends
        }

        this->discard();
        const auto differs = [](const char* saved, const char* got) {
            return saved != nullptr && (got == nullptr || std::strcmp(saved, got) != 0);
        };
        result.restore = true;
        result.power = snapshot.power;
        // Always re-send the mode: it keeps the climate mode correct when the OFF report was already applied.
        result.mode = snapshot.mode;
        if (differs(snapshot.fan, received.fan)) result.fan = snapshot.fan;
        if (differs(snapshot.vane, received.vane)) result.vane = snapshot.vane;
        if (differs(snapshot.wideVane, received.wideVane)) result.wide_vane = snapshot.wideVane;
        if (!std::isnan(snapshot.temperature) && snapshot.temperature > 0 &&
            (std::isnan(received.temperature) ||
             std::fabs(snapshot.temperature - received.temperature) > 0.01f)) {
            result.set_temperature = true;
            result.temperature = snapshot.temperature;
        }
        return result;
    }

private:
    bool enabled_ = true;
    bool valid_ = false;
    bool window_open_ = false;
    uint32_t saved_ms_ = 0;
    uint32_t window_ms_ = FUNCTIONS_RESTORE_WINDOW_MS;
    heatpumpSettings saved_{};
};

}  // namespace esphome::cn105
