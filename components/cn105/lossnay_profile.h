#pragma once
#include "protocol_profile.h"
#include "lossnay_profile_protocol.h"

namespace esphome::cn105 {

class LossnayProfile final : public Cn105ProtocolProfile {
 public:
    using Cn105ProtocolProfile::Cn105ProtocolProfile;
    uint8_t id() const override { return LOSSNAY_PROFILE; }
    const char* name() const override { return "Lossnay"; }
    bool supports(ProfileFeature feature) const override { return LossnayProtocol::supports(feature); }
    bool allow_operation(ProfileFeature feature) override;
    bool accepts_handshake(uint8_t command, uint8_t profile) const override {
        return LossnayProtocol::accepts_handshake(command, profile);
    }
    void reset() override { protocol_.reset(); }
    const char* mode_setting(const char* setting) const override;
    const char* fan_setting(const char* setting) const override;
    void decode_settings() override;
    void decode_status() override;
    bool decode_submode() override;
    void encode_control(uint8_t* packet) override;
    bool decode_functions(uint8_t code, const uint8_t* data, size_t length) override;
    bool encode_functions(uint8_t* first, uint8_t* second, const heatpumpFunctions& functions) override {
        return LossnayProtocol::encode_functions(first, second, functions);
    }
    void encode_run_states(uint8_t* packet) override { LossnayProtocol::encode_run_states(packet); }
    void reconcile_power_mode(heatpumpSettings& settings, bool update_current) override;
    void apply_received_settings(heatpumpSettings& settings) override;
    void apply_wanted_settings() override;
    void control_mode() override;
    void control_temperature() override { allow_operation(ProfileFeature::TEMPERATURE); }
    void control_swing() override { allow_operation(ProfileFeature::SWING); }
    bool accepts_remote_temperature(float) override { return allow_operation(ProfileFeature::REMOTE_TEMPERATURE); }
    void control_fan() override;
    void reconcile_fan(heatpumpSettings& settings, bool update_current) override;
    bool process_temperature_change(const climate::ClimateCall& call) override;
    void update_action() override;

 private:
    LossnayProtocol protocol_;
    bool target_warning_logged_{false};
};

}  // namespace esphome::cn105
