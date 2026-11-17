#pragma once

#include "cn105_protocol.h"

namespace cn105_protocol {

/// Hardware AUTO inside the band lets the head follow its shared outdoor unit.
/// An explicit HEAT or COOL request outside the band drives it toward the nearest edge.
enum class HeatCoolDrive : uint8_t { NONE, AUTO, HEAT, COOL };

/// Drive for a HEAT_COOL band [low, high].
/// - Entering: explicit HEAT/COOL when the room is `margin` past an edge, AUTO otherwise.
/// - AUTO -> HEAT/COOL once the room is `margin` past an edge and hardware AUTO is not
///   already running toward the band.
/// - HEAT/COOL -> AUTO once the room is back inside the band and the explicit drive
///   has run for `min_dwell_ms`. An explicit mode keeps the outdoor unit in its
///   direction even while idle, blocking heads that want the other one.
/// @param room            Room temperature (NaN when unknown)
/// @param current         Drive in force, NONE when entering HEAT_COOL
/// @param auto_dir        Direction hardware AUTO reports (only read in AUTO)
/// @param last_switch_ms  millis() of the last drive change
/// @return the drive to run (never NONE)
inline HeatCoolDrive heat_cool_drive(
    float room, float low, float high, HeatCoolDrive current, AutoDirection auto_dir,
    uint32_t now_ms, uint32_t last_switch_ms, float margin, uint32_t min_dwell_ms
) {
    if (std::isnan(room)) {
        return current == HeatCoolDrive::NONE ? HeatCoolDrive::AUTO : current;
    }
    const bool too_cold = room <= low - margin;
    const bool too_warm = room >= high + margin;
    const bool dwelled = static_cast<uint32_t>(now_ms - last_switch_ms) >= min_dwell_ms;
    switch (current) {
    case HeatCoolDrive::NONE:
        if (too_cold) return HeatCoolDrive::HEAT;
        if (too_warm) return HeatCoolDrive::COOL;
        return HeatCoolDrive::AUTO;
    case HeatCoolDrive::AUTO:
        if (too_cold && auto_dir != AutoDirection::HEAT) return HeatCoolDrive::HEAT;
        if (too_warm && auto_dir != AutoDirection::COOL) return HeatCoolDrive::COOL;
        return HeatCoolDrive::AUTO;
    case HeatCoolDrive::HEAT:
        return (dwelled && room >= low) ? HeatCoolDrive::AUTO : HeatCoolDrive::HEAT;
    case HeatCoolDrive::COOL:
        return (dwelled && room <= high) ? HeatCoolDrive::AUTO : HeatCoolDrive::COOL;
    }
    return current;
}

/// Single setpoint for hardware AUTO inside a HEAT_COOL band: the band edge on the
/// side AUTO is running (low while heating, high while cooling), so following the
/// outdoor unit's direction idles anywhere in the band. Before AUTO has reported a
/// direction, the midpoint keeps any first move inside the band.
/// @param last_dir  Last direction AUTO reported (UNKNOWN until it reports one)
inline float heat_cool_auto_setpoint(float low, float high, AutoDirection last_dir) {
    if (last_dir == AutoDirection::HEAT) return low;
    if (last_dir == AutoDirection::COOL) return high;
    return (low + high) / 2.0f;
}

struct HeatCoolCommand {
    HeatCoolDrive drive;
    float setpoint;
};

inline const char *heat_cool_mode_name(HeatCoolDrive drive) {
    if (drive == HeatCoolDrive::HEAT) return "HEAT";
    if (drive == HeatCoolDrive::COOL) return "COOL";
    return "AUTO";
}

// One instance per indoor head. The caller supplies the monotonic clock and the
// band in heat-pump degrees Celsius. Only returned commands go into wantedSettings.
class HeatCoolController {
 public:
    HeatCoolDrive drive() const { return drive_; }

    void direction_report(AutoDirection direction, uint32_t now, uint32_t poll_ms) {
        expire_direction(now, poll_ms);
        direction_ = direction;
        if (direction != AutoDirection::UNKNOWN) last_edge_ = direction;
        last_report_ms_ = now;
    }

    // A normal SET ACK is shared with other CN105 operations. A settings reply
    // echoes both the hardware mode and the representable target of this command.
    void await_user_readback(float encoded_target) { expected_target_ = encoded_target; }

    void settings_report(const char *mode, float target) {
        if (mode && target == expected_target_ && std::strcmp(mode, heat_cool_mode_name(drive_)) == 0)
            expected_target_ = NAN;
    }

    void adopt(const char *mode, float target, uint32_t now) {
        if (drive_ != HeatCoolDrive::NONE || mode == nullptr) return;
        if (std::strcmp(mode, "HEAT") == 0) drive_ = HeatCoolDrive::HEAT;
        else if (std::strcmp(mode, "COOL") == 0) drive_ = HeatCoolDrive::COOL;
        else if (std::strcmp(mode, "AUTO") == 0) drive_ = HeatCoolDrive::AUTO;
        else return;
        switched_ms_ = now;
        last_command_ms_ = now;
        last_target_ = target;
    }

    std::optional<HeatCoolCommand> update(float room, float low, float high, const char *reported_mode,
                                          bool pending, bool user_command, uint32_t now, uint32_t poll_ms) {
        expire_direction(now, poll_ms);
        if (!std::isfinite(low) || !std::isfinite(high) || low > high) return std::nullopt;
        if (!user_command && (pending || reported_mode == nullptr || !std::isfinite(room))) return std::nullopt;

        if (!user_command && !std::isnan(expected_target_)) {
            if (now - last_command_ms_ < REASSERT_MS) return std::nullopt;
            expected_target_ = NAN;
        }
        const auto next = heat_cool_drive(room, low, high, drive_, direction_, now, switched_ms_,
                                          SWITCH_MARGIN, user_command ? 0 : MIN_DWELL_MS);
        const bool changed = next != drive_;
        if (changed) {
            drive_ = next;
            switched_ms_ = now;
        }
        float target = heat_cool_auto_setpoint(low, high, last_edge_);
        if (next == HeatCoolDrive::HEAT) target = low;
        if (next == HeatCoolDrive::COOL) target = high;
        HeatCoolCommand command{next, target};
        if (!user_command && !changed) {
            if (std::strcmp(reported_mode, heat_cool_mode_name(next)) != 0) {
                if (now - last_command_ms_ < REASSERT_MS) return std::nullopt;
            } else if (last_target_ == target) {
                return std::nullopt;
            }
        }
        last_target_ = target;
        last_command_ms_ = now;
        return command;
    }

 private:
    static constexpr float SWITCH_MARGIN = 0.5f;
    static constexpr uint32_t MIN_DWELL_MS = 10 * 60 * 1000;
    static constexpr uint32_t REASSERT_MS = 60 * 1000;
    static constexpr uint32_t DIRECTION_TIMEOUT_MS = 60 * 1000;
    void expire_direction(uint32_t now, uint32_t poll_ms) {
        // Allow three slow poll cycles, with a one-minute floor on typical installations.
        const uint32_t age = now - last_report_ms_;
        if (age >= DIRECTION_TIMEOUT_MS && age / 3 >= poll_ms) {
            direction_ = last_edge_ = AutoDirection::UNKNOWN;
        }
    }

    HeatCoolDrive drive_ = HeatCoolDrive::NONE;
    AutoDirection direction_ = AutoDirection::UNKNOWN;
    AutoDirection last_edge_ = AutoDirection::UNKNOWN;
    uint32_t last_report_ms_ = 0;
    uint32_t switched_ms_ = 0;
    uint32_t last_command_ms_ = 0;
    float last_target_ = NAN;
    float expected_target_ = NAN;
};

}  // namespace cn105_protocol
