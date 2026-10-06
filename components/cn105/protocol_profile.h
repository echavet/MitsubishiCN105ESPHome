#pragma once

#include "cn105_types.h"
#include "profile_protocol.h"
#include <memory>

class heatpumpFunctions;

namespace esphome {
class CN105Climate;
namespace climate { class ClimateCall; }
namespace cn105 {

enum class ProfileFeature { TEMPERATURE, SWING, REMOTE_TEMPERATURE, AUXILIARY_CONTROLS, INSTALLER_MODE };

// Payload dialect strategy. CN105Climate owns transport, scheduling and publishing.
class Cn105ProtocolProfile {
 public:
    explicit Cn105ProtocolProfile(CN105Climate& climate) : hp_(climate) {}
    virtual ~Cn105ProtocolProfile() = default;
    virtual uint8_t id() const = 0;
    virtual const char* name() const = 0;
    virtual bool supports(ProfileFeature feature) const = 0;
    virtual bool allow_operation(ProfileFeature feature) { return supports(feature); }
    virtual bool accepts_handshake(uint8_t command, uint8_t profile) const = 0;
    virtual void reset() {}
    virtual const char* mode_setting(const char* setting) const = 0;
    virtual const char* fan_setting(const char* setting) const = 0;
    virtual const char* wide_vane_setting() { return nullptr; }
    virtual void decode_settings() = 0;
    virtual void decode_status() = 0;
    virtual heatpumpStatus decode_room_status(const uint8_t* data, const heatpumpStatus& previous) const;
    virtual std::string decode_error(const uint8_t* data) const { return CommonProtocol::decode_error(data); }
    virtual bool decode_functions(uint8_t code, const uint8_t* data, size_t length) { return false; }
    virtual bool decode_submode() = 0;
    virtual void decode_hvac_options() {}
    virtual void encode_control(uint8_t* packet) = 0;
    virtual void encode_remote_temperature(uint8_t* packet) {}
    virtual void encode_run_states(uint8_t* packet) {}
    virtual bool encode_functions(uint8_t* first, uint8_t* second, const heatpumpFunctions& functions) { return false; }
    virtual void apply_functions() {}
    virtual void reconcile_power_mode(heatpumpSettings& settings, bool update_current) = 0;
    virtual void apply_received_settings(heatpumpSettings& settings) = 0;
    virtual void apply_wanted_settings() = 0;
    virtual void apply_wanted_run_states() {}
    virtual void control_mode() = 0;
    virtual void control_fan() = 0;
    virtual void reconcile_fan(heatpumpSettings& settings, bool update_current) = 0;
    virtual void update_target_temperatures(float temperature) {}
    virtual bool process_temperature_change(const climate::ClimateCall& call) = 0;
    virtual void control_temperature() {}
    virtual float calculate_temperature_setting(float setting) const { return setting; }
    virtual void control_swing() {}
    virtual void set_vane_setting(const char* setting) {}
    virtual void set_wide_vane_setting(const char* setting) {}
    virtual void set_airflow_setting(const char* setting) {}
    virtual bool accepts_remote_temperature(float setting) { return allow_operation(ProfileFeature::REMOTE_TEMPERATURE); }
    virtual bool can_control_airflow() const { return false; }
    virtual void update_action() = 0;
    virtual void restore_setpoints() {}
    virtual void save_setpoints() {}
    virtual void sanitize_setpoints() {}

 protected:
    CN105Climate& hp_;
};

std::unique_ptr<Cn105ProtocolProfile> make_protocol_profile(CN105Climate& climate, bool lossnay);

}  // namespace cn105
}  // namespace esphome
