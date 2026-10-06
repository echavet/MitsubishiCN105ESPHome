#include <gtest/gtest.h>
#include "heatpump_profile_protocol.h"
#include "lossnay_profile_protocol.h"
#include "profile_protocol.h"
#include <array>

using namespace esphome::climate;
using esphome::cn105::HeatPumpProtocol;
using esphome::cn105::LossnayProtocol;

TEST(HeatPumpProfile, PartialWritesPreserveMissingFields) {
    heatpumpSettings current{}, incoming{};
    current.power = "ON";
    current.mode = "HEAT";
    incoming.mode = "COOL";
    EXPECT_EQ(HeatPumpProtocol::reconcile_power_mode(incoming, current, CLIMATE_MODE_HEAT, false, true),
              CLIMATE_MODE_HEAT);
    EXPECT_STREQ(current.power, "ON");
    EXPECT_STREQ(current.mode, "COOL");
    incoming = {};
    incoming.power = "OFF";
    EXPECT_EQ(HeatPumpProtocol::reconcile_power_mode(incoming, current, CLIMATE_MODE_HEAT, false, true),
              CLIMATE_MODE_OFF);
    EXPECT_STREQ(current.mode, "COOL");
}

TEST(HeatPumpProfile, OptimisticWritesLeaveReceivedSettingsIntact) {
    heatpumpSettings current{}, incoming{};
    current.power = "OFF";
    current.mode = "HEAT";
    incoming.power = "ON";
    incoming.mode = "COOL";
    EXPECT_EQ(HeatPumpProtocol::reconcile_power_mode(incoming, current, CLIMATE_MODE_COOL, false, false),
              CLIMATE_MODE_COOL);
    EXPECT_STREQ(current.power, "OFF");
    EXPECT_STREQ(current.mode, "HEAT");
}

TEST(HeatPumpProfile, MissingAndUnknownModesAreSafe) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode(nullptr, nullptr, CLIMATE_MODE_HEAT, true), CLIMATE_MODE_HEAT);
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", nullptr, CLIMATE_MODE_COOL, false), CLIMATE_MODE_COOL);
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "UNKNOWN", CLIMATE_MODE_DRY, false), CLIMATE_MODE_DRY);
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("OFF", nullptr, CLIMATE_MODE_HEAT_COOL, true), CLIMATE_MODE_OFF);
}

TEST(HeatPumpProfile, CompressorIdleFilteringRemainsConfigurable) {
    uint8_t data[16]{};
    data[3] = 63;
    EXPECT_EQ(HeatPumpProtocol::compressor_frequency(data, false), 0);
    EXPECT_EQ(HeatPumpProtocol::compressor_frequency(data, true), 63);
    data[4] = 1;
    EXPECT_EQ(HeatPumpProtocol::compressor_frequency(data, false), 63);
    EXPECT_EQ(HeatPumpProtocol::compressor_frequency(data, true), 63);
}

TEST(HeatPumpProfile, OperatingAndStageFallback) {
    EXPECT_EQ(HeatPumpProtocol::operating_action(true, false, nullptr, CLIMATE_ACTION_HEATING), CLIMATE_ACTION_HEATING);
    EXPECT_EQ(HeatPumpProtocol::operating_action(false, true, "HIGH", CLIMATE_ACTION_HEATING), CLIMATE_ACTION_HEATING);
    EXPECT_EQ(HeatPumpProtocol::operating_action(false, false, "HIGH", CLIMATE_ACTION_HEATING), CLIMATE_ACTION_IDLE);
    EXPECT_EQ(HeatPumpProtocol::operating_action(false, true, "IDLE", CLIMATE_ACTION_COOLING), CLIMATE_ACTION_IDLE);
    EXPECT_EQ(HeatPumpProtocol::operating_action(false, true, nullptr, CLIMATE_ACTION_COOLING), CLIMATE_ACTION_IDLE);
}

TEST(HeatPumpProfile, EncodingBRemainsLatchedThroughEncodingAFallback) {
    uint8_t data[16]{};
    bool encoding_b = false, latched = false;
    data[11] = 173;
    EXPECT_FLOAT_EQ(HeatPumpProtocol::decode_temperature(data, 18, false, encoding_b, latched), 22.5);
    EXPECT_TRUE(encoding_b);
    EXPECT_TRUE(latched);
    data[11] = 0;
    data[5] = 10;
    EXPECT_FLOAT_EQ(HeatPumpProtocol::decode_temperature(data, 18, false, encoding_b, latched), 21);
    EXPECT_TRUE(encoding_b);
    EXPECT_TRUE(latched);
    data[11] = 0x80;
    EXPECT_FLOAT_EQ(HeatPumpProtocol::decode_temperature(data, 24, false, encoding_b, latched), 24);
    data[11] = 0;
    data[5] = 0x7F;
    EXPECT_FLOAT_EQ(HeatPumpProtocol::decode_temperature(data, 24, false, encoding_b, latched), 24);
}

TEST(HeatPumpProfile, SpecialTemperatureTableTakesPrecedence) {
    uint8_t data[16]{};
    bool encoding_b = false, latched = false;
    data[5] = 0x1A;
    data[11] = 0x80;
    EXPECT_FLOAT_EQ(HeatPumpProtocol::decode_temperature(data, 18, true, encoding_b, latched), 21.5);
    EXPECT_FALSE(latched);
}

TEST(HeatPumpProfile, TemperatureOnlyWriteRetainsStickyVane) {
    wantedHeatpumpSettings wanted{};
    wanted.last_user_vane = VANE_MAP[5];
    wanted.last_user_temperature = 24;
    wanted.resetSettings();
    wanted.temperature = 22.5;
    uint8_t packet[22]{};
    HeatPumpProtocol::encode_control(packet, wanted, {}, true, false, false, false);
    EXPECT_EQ(packet[6], CONTROL_PACKET_1[2] | CONTROL_PACKET_1[4]);
    EXPECT_EQ(packet[12], VANE[5]);
    EXPECT_EQ(packet[19], 173);
    EXPECT_EQ(packet[10], 0);
    EXPECT_FLOAT_EQ(wanted.last_user_temperature, 24);
}

TEST(HeatPumpProfile, ModeOnlyWriteDoesNotEmitTemperature) {
    wantedHeatpumpSettings wanted{};
    wanted.mode = "DRY";
    wanted.power = "ON";
    wanted.last_user_temperature = 24;
    uint8_t packet[22]{};
    HeatPumpProtocol::encode_control(packet, wanted, {}, true, false, false, false);
    EXPECT_EQ(packet[6], CONTROL_PACKET_1[0] | CONTROL_PACKET_1[1]);
    EXPECT_EQ(packet[8], 1);
    EXPECT_EQ(packet[9], MODE[1]);
    EXPECT_EQ(packet[10], 0);
    EXPECT_EQ(packet[19], 0);
}

TEST(HeatPumpProfile, VaneOverrideAndSplitHorizontalPacket) {
    wantedHeatpumpSettings wanted{};
    wanted.last_user_vane = VANE_MAP[5];
    wanted.vane = VANE_MAP[2];
    wanted.wideVane = WIDEVANE_MAP[1];
    uint8_t packet[22]{};
    HeatPumpProtocol::encode_control(packet, wanted, {}, false, false, true, true);
    EXPECT_EQ(packet[12], VANE[2]);
    EXPECT_EQ(packet[18], WIDEVANE[1] | 0x80);
    EXPECT_EQ(packet[16], WIDEVANE[1]);
    EXPECT_EQ(packet[7], CONTROL_PACKET_2[0]);
}

TEST(HeatPumpProfile, BothTemperatureEncodingsAndSpecialTable) {
    wantedHeatpumpSettings wanted{};
    wanted.temperature = 22;
    uint8_t a[22]{}, b[22]{}, special[22]{};
    HeatPumpProtocol::encode_control(a, wanted, {}, false, false, false, false);
    HeatPumpProtocol::encode_control(b, wanted, {}, true, false, false, false);
    wanted.temperature = 21.5;
    HeatPumpProtocol::encode_control(special, wanted, {}, true, true, false, false);
    EXPECT_EQ(a[10], 9);
    EXPECT_EQ(b[19], 172);
    EXPECT_EQ(special[10], 0x1A);
    EXPECT_EQ(special[19], 0);
}

TEST(HeatPumpProfile, RemoteTemperatureEncodingAndInternalSensorFallback) {
    uint8_t remote[22]{}, internal[22]{};
    HeatPumpProtocol::encode_remote_temperature(remote, 21.5);
    HeatPumpProtocol::encode_remote_temperature(internal, 0);
    EXPECT_EQ(remote[5], 7);
    EXPECT_EQ(remote[6], 1);
    EXPECT_EQ(remote[7], 27);
    EXPECT_EQ(remote[8], 171);
    EXPECT_EQ(internal[6], 0);
    EXPECT_EQ(internal[8], 0x80);
}

TEST(LossnayProfile, RepeatedActualModePollsPublishOnlyActionTransitions) {
    LossnayProtocol profile;
    uint8_t data[16]{};
    data[0] = 9;
    data[7] = 0x40;
    ClimateAction action = CLIMATE_ACTION_IDLE;
    EXPECT_TRUE(profile.update_actual_action(data, "ON", CLIMATE_MODE_AUTO, action));
    EXPECT_EQ(action, CLIMATE_ACTION_HEATING);
    for (int i = 0; i < 10; ++i) EXPECT_FALSE(profile.update_actual_action(data, "ON", CLIMATE_MODE_AUTO, action));
    data[8] = 1;
    EXPECT_TRUE(profile.update_actual_action(data, "ON", CLIMATE_MODE_AUTO, action));
    EXPECT_EQ(action, CLIMATE_ACTION_FAN);
    EXPECT_FALSE(profile.update_actual_action(data, "ON", CLIMATE_MODE_AUTO, action));
}

TEST(LossnayProfile, UnknownModeValidityTransitionsArePublishedOnce) {
    LossnayProtocol profile;
    uint8_t data[16]{};
    data[8] = 0xFF;
    ClimateAction action = CLIMATE_ACTION_IDLE;
    EXPECT_FALSE(profile.update_actual_action(data, "ON", CLIMATE_MODE_AUTO, action));
    data[8] = 1;
    EXPECT_TRUE(profile.update_actual_action(data, "ON", CLIMATE_MODE_AUTO, action));
    data[8] = 2;
    EXPECT_TRUE(profile.update_actual_action(data, "ON", CLIMATE_MODE_AUTO, action));
    EXPECT_EQ(action, CLIMATE_ACTION_IDLE);
    data[8] = 3;
    EXPECT_FALSE(profile.update_actual_action(data, "ON", CLIMATE_MODE_AUTO, action));
}

TEST(LossnayProfile, ActualModeChangesDoNotRepublishManualOrOffActions) {
    LossnayProtocol profile;
    uint8_t data[16]{};
    ClimateAction action = CLIMATE_ACTION_HEATING;
    EXPECT_FALSE(profile.update_actual_action(data, "ON", CLIMATE_MODE_HEAT, action));
    data[8] = 1;
    EXPECT_FALSE(profile.update_actual_action(data, "ON", CLIMATE_MODE_HEAT, action));
    action = CLIMATE_ACTION_OFF;
    data[8] = 0;
    EXPECT_FALSE(profile.update_actual_action(data, "OFF", CLIMATE_MODE_AUTO, action));
}

TEST(LossnayProfile, ReconnectAndEnteringAutoInvalidateActualMode) {
    LossnayProtocol profile;
    uint8_t data[16]{};
    data[8] = 1;
    profile.decode_actual_mode(data);
    EXPECT_EQ(profile.action("ON", CLIMATE_MODE_AUTO), CLIMATE_ACTION_FAN);
    profile.reset();
    EXPECT_EQ(profile.action("ON", CLIMATE_MODE_AUTO), CLIMATE_ACTION_IDLE);
    profile.decode_actual_mode(data);
    wantedHeatpumpSettings wanted{};
    EXPECT_TRUE(profile.command_mode(CLIMATE_MODE_AUTO, wanted));
    EXPECT_EQ(profile.action("ON", CLIMATE_MODE_AUTO), CLIMATE_ACTION_IDLE);
}

TEST(LossnayProfile, CachesAreIndependentPerInstance) {
    LossnayProtocol first, second;
    uint8_t data[16]{};
    first.decode_actual_mode(data);
    EXPECT_EQ(first.action("ON", CLIMATE_MODE_AUTO), CLIMATE_ACTION_HEATING);
    EXPECT_EQ(second.action("ON", CLIMATE_MODE_AUTO), CLIMATE_ACTION_IDLE);
}

TEST(LossnayProfile, EveryOperatingModeIncludesPowerOnAfterRapidOff) {
    for (auto mode : {CLIMATE_MODE_HEAT, CLIMATE_MODE_FAN_ONLY, CLIMATE_MODE_AUTO}) {
        LossnayProtocol profile;
        wantedHeatpumpSettings wanted{};
        EXPECT_TRUE(profile.command_mode(CLIMATE_MODE_OFF, wanted));
        EXPECT_TRUE(profile.command_mode(mode, wanted));
        uint8_t packet[22]{};
        LossnayProtocol::encode_control(packet, wanted);
        EXPECT_EQ(packet[6], cn105_protocol::LOSSNAY_POWER_MASK | cn105_protocol::LOSSNAY_MODE_MASK);
        EXPECT_EQ(packet[8], 1);
        EXPECT_EQ(packet[10], mode == CLIMATE_MODE_HEAT ? 0 : mode == CLIMATE_MODE_FAN_ONLY ? 1 : 2);
    }
}

TEST(LossnayProfile, UnsupportedModesDoNotChangeWantedSettings) {
    LossnayProtocol profile;
    wantedHeatpumpSettings wanted{};
    for (auto mode : {CLIMATE_MODE_COOL, CLIMATE_MODE_DRY, CLIMATE_MODE_HEAT_COOL}) {
        EXPECT_FALSE(profile.command_mode(mode, wanted));
        EXPECT_EQ(wanted.power, nullptr);
        EXPECT_EQ(wanted.mode, nullptr);
    }
}

TEST(LossnayProfile, HeatPumpFieldsNeverEnterControlPackets) {
    wantedHeatpumpSettings wanted{};
    wanted.temperature = 22;
    wanted.vane = "SWING";
    wanted.wideVane = "SWING";
    uint8_t packet[22]{};
    LossnayProtocol::encode_control(packet, wanted);
    EXPECT_EQ(packet[6], 0);
    EXPECT_EQ(packet[7], 0);
    EXPECT_EQ(packet[9], 0);
    EXPECT_EQ(packet[12], 0);
    EXPECT_EQ(packet[18], 0);
    EXPECT_EQ(packet[19], 0);
}

TEST(LossnayProfile, SettingsFallbackPreservesConfirmedFieldsOnly) {
    uint8_t data[16]{};
    data[3] = data[5] = data[6] = 0xFF;
    heatpumpSettings previous{};
    previous.power = "ON";
    previous.mode = "FAN";
    previous.fan = "3";
    auto settings = LossnayProtocol::decode_settings(data, previous);
    EXPECT_STREQ(settings.power, "ON");
    EXPECT_STREQ(settings.mode, "FAN");
    EXPECT_STREQ(settings.fan, "3");
    settings = LossnayProtocol::decode_settings(data, {});
    EXPECT_STREQ(settings.power, "OFF");
    EXPECT_STREQ(settings.mode, "AUTO");
    EXPECT_STREQ(settings.fan, "1");
}

TEST(LossnayProfile, PartialWritesAreNullSafeAndOptimisticWritesDoNotChangeCache) {
    LossnayProtocol profile;
    heatpumpSettings current{}, incoming{};
    current.power = "ON";
    current.mode = "HEAT";
    incoming.mode = "AUTO";
    EXPECT_EQ(profile.reconcile_power_mode(incoming, current, CLIMATE_MODE_HEAT, false), CLIMATE_MODE_HEAT);
    EXPECT_STREQ(current.mode, "HEAT");
    EXPECT_EQ(profile.reconcile_power_mode(incoming, current, CLIMATE_MODE_HEAT, true), CLIMATE_MODE_HEAT);
    EXPECT_STREQ(current.mode, "AUTO");
    incoming = {};
    incoming.power = "OFF";
    EXPECT_EQ(profile.reconcile_power_mode(incoming, current, CLIMATE_MODE_HEAT, true), CLIMATE_MODE_OFF);
    EXPECT_STREQ(current.mode, "AUTO");
}

TEST(LossnayProfile, StandardProfileHandshakeOnly) {
    EXPECT_TRUE(LossnayProtocol::accepts_handshake(0x7A, 0x34));
    EXPECT_FALSE(LossnayProtocol::accepts_handshake(0x7A, 0x30));
    EXPECT_FALSE(LossnayProtocol::accepts_handshake(0x7B, 0x34));
}

TEST(LossnayProfile, FanSpeedsUseProductionMappings) {
    for (auto mode : {CLIMATE_FAN_LOW, CLIMATE_FAN_MEDIUM, CLIMATE_FAN_MIDDLE, CLIMATE_FAN_HIGH}) {
        const auto setting = LossnayProtocol::fan_setting(mode);
        ASSERT_NE(setting, nullptr);
        EXPECT_EQ(LossnayProtocol::fan_mode(setting), mode);
        wantedHeatpumpSettings wanted{};
        wanted.fan = setting;
        uint8_t packet[22]{};
        LossnayProtocol::encode_control(packet, wanted);
        EXPECT_EQ(packet[6], cn105_protocol::LOSSNAY_FAN_MASK);
        EXPECT_GE(packet[11], 1);
        EXPECT_LE(packet[11], 4);
    }
    EXPECT_EQ(LossnayProtocol::fan_setting(CLIMATE_FAN_AUTO), nullptr);
    EXPECT_EQ(LossnayProtocol::fan_setting(CLIMATE_FAN_QUIET), nullptr);
    EXPECT_FALSE(LossnayProtocol::fan_mode(nullptr));
    EXPECT_FALSE(LossnayProtocol::fan_mode("AUTO"));
}

TEST(LossnayProfile, OptionalOperationsAreRejectedByProductionCapabilities) {
    using esphome::cn105::ProfileFeature;
    for (auto feature : {ProfileFeature::TEMPERATURE, ProfileFeature::SWING, ProfileFeature::REMOTE_TEMPERATURE,
                         ProfileFeature::AUXILIARY_CONTROLS, ProfileFeature::INSTALLER_MODE}) {
        EXPECT_FALSE(LossnayProtocol::supports(feature));
    }
}

TEST(HeatPumpProfile, AirflowControlRequiresActiveISee) {
    for (bool iSee : {false, true}) {
        wantedHeatpumpSettings wanted{};
        heatpumpSettings current{};
        wanted.wideVane = WIDEVANE_MAP[7];
        current.wideVane = WIDEVANE_MAP[3];
        current.iSee = iSee;
        uint8_t packet[22]{};
        HeatPumpProtocol::encode_control(packet, wanted, current, false, false, true, true);
        const auto expected = iSee ? WIDEVANE[7] : WIDEVANE[3];
        EXPECT_EQ(packet[18], expected | 0x80);
        EXPECT_EQ(packet[16], expected);
        EXPECT_EQ(packet[7], CONTROL_PACKET_2[0]);
        EXPECT_STREQ(wanted.wideVane, iSee ? WIDEVANE_MAP[7] : current.wideVane);
    }
}

TEST(HeatPumpProfile, AirflowControlWithoutKnownPositionIsNullSafe) {
    wantedHeatpumpSettings wanted{};
    heatpumpSettings current{};
    wanted.wideVane = WIDEVANE_MAP[7];
    uint8_t packet[22]{};
    HeatPumpProtocol::encode_control(packet, wanted, current, false, false, true, true);
    EXPECT_EQ(wanted.wideVane, nullptr);
    EXPECT_EQ(packet[7], 0);
    EXPECT_EQ(packet[16], 0);
    EXPECT_EQ(packet[18], 0);
}

TEST(CommonProfile, RoomStatusPreservesOtherReportsAndDecodesRuntime) {
    using esphome::cn105::CommonProtocol;
    heatpumpStatus previous{};
    previous.operating = true;
    previous.compressorFrequency = 45;
    previous.inputPower = 1234;
    previous.kWh = 234;
    previous.timers.onMinutesSet = 60;
    uint8_t data[16]{};
    data[3] = ROOM_TEMP[12];
    data[5] = 160;
    data[11] = 1; data[12] = 2; data[13] = 3;
    auto status = CommonProtocol::decode_room_status(data, previous);
    EXPECT_FLOAT_EQ(status.roomTemperature, ROOM_TEMP_MAP[12]);
    EXPECT_FLOAT_EQ(status.outsideAirTemperature, 16);
    EXPECT_FLOAT_EQ(status.runtimeHours, 66051.0f / 60);
    EXPECT_TRUE(status.operating);
    EXPECT_FLOAT_EQ(status.compressorFrequency, 45);
    EXPECT_FLOAT_EQ(status.inputPower, 1234);
    EXPECT_FLOAT_EQ(status.kWh, 234);
    EXPECT_EQ(status.timers.onMinutesSet, 60);
    EXPECT_TRUE(std::isnan(previous.roomTemperature));
}

TEST(CommonProfile, RoomEncodingBAndUnknownEncodingAFallback) {
    using esphome::cn105::CommonProtocol;
    heatpumpStatus previous{};
    previous.roomTemperature = 22.5;
    uint8_t data[16]{};
    data[3] = 0xFF;
    data[6] = 171;
    EXPECT_FLOAT_EQ(CommonProtocol::decode_room_status(data, previous).roomTemperature, 21.5);
    data[6] = 0;
    for (uint8_t outside : {0, 1}) {
        data[5] = outside;
        auto status = CommonProtocol::decode_room_status(data, previous);
        EXPECT_FLOAT_EQ(status.roomTemperature, 22.5);
        EXPECT_TRUE(std::isnan(status.outsideAirTemperature));
    }
}

TEST(CommonProfile, ErrorAvailabilityBitIsNotAnError) {
    using esphome::cn105::CommonProtocol;
    uint8_t data[16]{};
    EXPECT_EQ(CommonProtocol::decode_error(data), "No Error");
    data[4] = 0x80;
    EXPECT_EQ(CommonProtocol::decode_error(data), "No Error");
    data[4] = 0x92; data[5] = 0xAB;
    EXPECT_EQ(CommonProtocol::decode_error(data), "Error 0x12 sub 0xAB");
    data[4] = 0x80; data[5] = 1;
    EXPECT_EQ(CommonProtocol::decode_error(data), "Error 0x00 sub 0x01");
}

TEST(HeatPumpProfile, FunctionResponsesRejectInvalidPayloadsWithoutMutation) {
    heatpumpFunctions functions;
    uint8_t raw[15]; std::fill_n(raw, 15, 0x09);
    functions.setData1(raw); functions.setData2(raw);
    uint8_t data[16]{}; data[0] = 0x20;
    EXPECT_FALSE(HeatPumpProtocol::decode_functions(functions, 0x20, data, sizeof(data)));
    data[1] = 0x04;
    EXPECT_FALSE(HeatPumpProtocol::decode_functions(functions, 0x20, data, 15));
    EXPECT_FALSE(HeatPumpProtocol::decode_functions(functions, 0x22, data, sizeof(data)));
    data[0] = 0x19;
    EXPECT_FALSE(HeatPumpProtocol::decode_functions(functions, 0x19, data, sizeof(data)));
    functions.getData1(raw);
    for (auto byte : raw) EXPECT_EQ(byte, 0x09);
    functions.getData2(raw);
    for (auto byte : raw) EXPECT_EQ(byte, 0x09);
    EXPECT_TRUE(functions.isValid());
}

TEST(HeatPumpProfile, FunctionsWithZeroValuesStillAnnounceSupport) {
    heatpumpFunctions functions;
    uint8_t data[16]{};
    data[0] = 0x20; data[1] = (101 - 100) << 2;
    EXPECT_TRUE(HeatPumpProtocol::decode_functions(functions, 0x20, data, sizeof(data)));
    EXPECT_FALSE(functions.isValid());
    EXPECT_TRUE(functions.getAllCodes().valid[0]);
    EXPECT_EQ(functions.getValue(101), 0);
    data[0] = 0x22; data[1] = ((128 - 100) << 2) | 3;
    EXPECT_TRUE(HeatPumpProtocol::decode_functions(functions, 0x22, data, sizeof(data)));
    EXPECT_TRUE(functions.isValid());
    EXPECT_EQ(functions.getValue(128), 3);
}

TEST(HeatPumpProfile, FunctionWritesRequireBothHalvesAndUseProductionPayloads) {
    heatpumpFunctions functions;
    std::array<uint8_t, 22> first{}, second{};
    EXPECT_FALSE(HeatPumpProtocol::encode_functions(first.data(), second.data(), functions));
    EXPECT_EQ(first, (std::array<uint8_t, 22>{}));
    uint8_t part1[15]{}, part2[15]{};
    part1[0] = 0x05; part2[14] = 0x73;
    functions.setData1(part1);
    EXPECT_FALSE(HeatPumpProtocol::encode_functions(first.data(), second.data(), functions));
    functions.setData2(part2);
    EXPECT_TRUE(HeatPumpProtocol::encode_functions(first.data(), second.data(), functions));
    EXPECT_EQ(first[5], 0x1F); EXPECT_EQ(second[5], 0x21);
    EXPECT_TRUE(std::equal(part1, part1 + 15, first.begin() + 6));
    EXPECT_TRUE(std::equal(part2, part2 + 15, second.begin() + 6));
}

TEST(HeatPumpProfile, FunctionEqualityComparesOnlyTheStoredBytes) {
    heatpumpFunctions first, second;
    uint8_t data[15]{}; data[0] = 0x05;
    first.setData1(data); first.setData2(data);
    second.setData1(data); second.setData2(data);
    EXPECT_TRUE(first == second);
    EXPECT_TRUE(second.setValue(101, 2));
    EXPECT_TRUE(first != second);
}

TEST(HeatPumpProfile, RunStateWritesUseMasksAndPreserveMissingFields) {
    heatpumpRunStates current{};
    current.air_purifier = 0; current.night_mode = 1; current.circulator = 0;
    current.airflow_control = "EVEN";
    wantedHeatpumpRunStates wanted{};
    wanted.air_purifier = 1; wanted.night_mode = 0; wanted.airflow_control = "DIRECT";
    uint8_t packet[22]{};
    HeatPumpProtocol::encode_run_states(packet, wanted, current);
    EXPECT_EQ(packet[5], 8);
    EXPECT_EQ(packet[6], RUN_STATE_PACKET_1[4]);
    EXPECT_EQ(packet[7], RUN_STATE_PACKET_2[1] | RUN_STATE_PACKET_2[2]);
    EXPECT_EQ(packet[11], 2); EXPECT_EQ(packet[17], 1); EXPECT_EQ(packet[18], 0);
    auto optimistic = HeatPumpProtocol::optimistic_run_states(wanted, current);
    EXPECT_EQ(optimistic.air_purifier, 1); EXPECT_EQ(optimistic.night_mode, 0);
    EXPECT_EQ(optimistic.circulator, 0); EXPECT_STREQ(optimistic.airflow_control, "DIRECT");
    EXPECT_EQ(current.air_purifier, 0); EXPECT_EQ(current.night_mode, 1);
    EXPECT_STREQ(current.airflow_control, "EVEN");
}

TEST(HeatPumpProfile, UnchangedAndUnknownRunStatesDoNotSetWriteMasks) {
    heatpumpRunStates current{};
    current.air_purifier = 1; current.night_mode = 0; current.circulator = 1;
    wantedHeatpumpRunStates wanted{};
    wanted.air_purifier = 1; wanted.night_mode = 0; wanted.circulator = 1;
    wanted.airflow_control = "UNKNOWN";
    uint8_t packet[22]{};
    HeatPumpProtocol::encode_run_states(packet, wanted, current);
    EXPECT_EQ(packet[6], 0); EXPECT_EQ(packet[7], 0);
    wanted.resetSettings();
    EXPECT_EQ(HeatPumpProtocol::optimistic_run_states(wanted, current), current);
    current.air_purifier = -1; wanted.air_purifier = 0;
    HeatPumpProtocol::encode_run_states(packet, wanted, current);
    EXPECT_EQ(packet[7], RUN_STATE_PACKET_2[1]);
}

TEST(HeatPumpProfile, AirflowAndRemoteTemperatureAcceptance) {
    heatpumpSettings current{};
    EXPECT_FALSE(HeatPumpProtocol::can_control_airflow(current));
    current.wideVane = "SWING";
    EXPECT_FALSE(HeatPumpProtocol::can_control_airflow(current));
    current.wideVane = WIDEVANE_MAP[7];
    EXPECT_TRUE(HeatPumpProtocol::can_control_airflow(current));
    EXPECT_FALSE(HeatPumpProtocol::accepts_remote_temperature(NAN));
    EXPECT_TRUE(HeatPumpProtocol::accepts_remote_temperature(0));
    EXPECT_TRUE(HeatPumpProtocol::accepts_remote_temperature(21.5));
}

TEST(LossnayProfile, UnsupportedFunctionAndRunStatePayloadsRemainUntouched) {
    heatpumpFunctions functions;
    uint8_t raw[15]{}; raw[0] = 0x05;
    functions.setData1(raw); functions.setData2(raw);
    uint8_t data[16]{}; data[0] = 0x20; data[1] = 0x09;
    EXPECT_FALSE(LossnayProtocol::decode_functions(functions, 0x20, data, sizeof(data)));
    functions.getData1(raw); EXPECT_EQ(raw[0], 0x05);
    std::array<uint8_t, 22> first, second;
    first.fill(0xA5); second.fill(0x5A);
    auto original_first = first, original_second = second;
    EXPECT_FALSE(LossnayProtocol::encode_functions(first.data(), second.data(), functions));
    LossnayProtocol::encode_run_states(first.data());
    EXPECT_EQ(first, original_first); EXPECT_EQ(second, original_second);
}
