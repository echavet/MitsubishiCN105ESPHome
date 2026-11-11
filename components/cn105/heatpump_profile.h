#pragma once
#include "protocol_profile.h"
#include "heatpump_profile_protocol.h"
#include "esphome/core/preferences.h"

namespace esphome::cn105 {

class HeatPumpProfile final : public Cn105ProtocolProfile {
 public:
    using Cn105ProtocolProfile::Cn105ProtocolProfile;
    uint8_t id() const override { return HEATPUMP_PROFILE; }
    const char* name() const override { return "Heat pump"; }
    bool supports(ProfileFeature) const override { return true; }
    bool accepts_handshake(uint8_t command, uint8_t) const override { return command == 0x7A || command == 0x7B; }
    const char* mode_setting(const char* setting) const override;
    const char* fan_setting(const char* setting) const override;
    const char* wide_vane_setting() override;
    void decode_settings() override;
    void decode_status() override;
    bool decode_submode() override;
    void decode_hvac_options() override;
    bool decode_functions(uint8_t code, const uint8_t* data, size_t length) override;
    void encode_control(uint8_t* packet) override;
    void encode_remote_temperature(uint8_t* packet) override;
    void encode_run_states(uint8_t* packet) override;
    bool encode_functions(uint8_t* first, uint8_t* second, const heatpumpFunctions& functions) override;
    void apply_functions() override;
    void reconcile_power_mode(heatpumpSettings& settings, bool update_current) override;
    void apply_received_settings(heatpumpSettings& settings) override;
    void apply_wanted_settings() override;
    void apply_wanted_run_states() override;
    void control_mode() override;
    void control_fan() override;
    void reconcile_fan(heatpumpSettings& settings, bool update_current) override;
    void update_target_temperatures(float temperature) override;
    bool process_temperature_change(const climate::ClimateCall& call) override;
    void control_temperature() override;
    float calculate_temperature_setting(float setting) const override;
    void control_swing() override;
    void set_vane_setting(const char* setting) override;
    void set_wide_vane_setting(const char* setting) override;
    void set_airflow_setting(const char* setting) override;
    bool accepts_remote_temperature(float setting) override;
    bool can_control_airflow() const override;
    void update_action() override;
    void restore_setpoints() override;
    void save_setpoints() override;
    void sanitize_setpoints() override;
    void reevaluate_heat_cool() override;

 private:
    void debug_functions(const uint8_t* packet, unsigned int length);
    bool use_temperature_encoding_b_ = false;
    bool use_temperature_encoding_b_latched_ = false;
    bool wide_vane_adjust_ = false;
    uint32_t last_dual_setpoint_change_ms_ = 0;
    char last_dual_setpoint_side_ = 'N';
    using SetpointState = HeatPumpProtocol::SetpointState;
    ESPPreferenceObject setpoint_pref_;
    bool setpoint_pref_ready_ = false;
    void handleDualSetpointBoth(float low, float high);
    void handleDualSetpointLowOnly(float low);
    void handleDualSetpointHighOnly(float high);
    void handleSingleTargetInAutoOrDry(float requested);
    void checkVaneSettings(heatpumpSettings& settings, bool updateCurrentSettings = true);
    void checkWideVaneSettings(heatpumpSettings& settings, bool updateCurrentSettings = true);
    void updateExtraSelectComponents(heatpumpSettings& settings);
    float decodeSettingsTemperature(const uint8_t* data);
    bool hasPendingUserTemperature() const;
    bool isWithinPostSendGrace() const;
    bool disagreesWithLastUserSetpoint(float incoming) const;
    bool shouldApplyIncomingSetpoint(const heatpumpSettings& settings);
    bool shouldIgnoreIncomingVane(const heatpumpSettings& settings) const;

    void setActionIfOperatingTo(climate::ClimateAction action);
    bool setActionFromAutoDirection();

    // --- HEAT_COOL with dual_setpoint: hardware AUTO in the band, HEAT/COOL outside ---
    // Heads on a shared outdoor unit run one direction at a time, and an idle head in
    // explicit COOL still holds it in cooling. Hardware AUTO follows the outdoor unit,
    // so HEAT_COOL runs AUTO inside the band with the setpoint on the edge AUTO is
    // running toward (see cn105_protocol::heat_cool_drive).
    static constexpr float HEAT_COOL_SWITCH_MARGIN = 0.5f;
    static constexpr uint32_t HEAT_COOL_MIN_DWELL_MS = 10 * 60 * 1000;
    static constexpr uint32_t HEAT_COOL_REASSERT_MS = 60 * 1000;
    cn105_protocol::HeatCoolDrive heat_cool_drive_{ cn105_protocol::HeatCoolDrive::NONE };
    cn105_protocol::AutoDirection heat_cool_auto_dir_{ cn105_protocol::AutoDirection::UNKNOWN };
    uint32_t heat_cool_switch_ms_ = 0;
    uint32_t heat_cool_sent_ms_ = 0;
    bool managesHeatCool() const;
    void setHeatCoolDrive(cn105_protocol::HeatCoolDrive next);
    void applyHeatCoolDrive(bool user_command);
    void adoptReportedHeatCoolDrive(const char* reported_mode);
    float heatCoolSetpoint() const;
    void setWantedTemperature(float setting);
};

}  // namespace esphome::cn105
