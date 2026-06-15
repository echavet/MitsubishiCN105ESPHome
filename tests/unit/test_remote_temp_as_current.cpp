/// test_remote_temp_as_current.cpp — Tests for displaying the remote
/// (room) temperature as the climate current_temperature when a
/// remote_temperature_source is active (opt-in use_as_current_temperature).
///
/// Background: a CN105 unit driven from a remote sensor uses the remote value
/// for *control*, but the unit still reports its own internal probe in the
/// settings packet, and setCurrentTemperature() (utils.cpp) normally pushes
/// that probe to the climate entity. So the HA card shows the unit's intake
/// temperature, not the room. When remote_temperature_source.use_as_current_temperature
/// is enabled, setCurrentTemperature() surfaces the active remote value instead.
///
/// remoteTemperature_ is reset to 0 on timeout/revert (see set_remote_temperature),
/// so the selection automatically falls back to the unit probe when no remote
/// temp is in use. This mirrors that selection (the production normalize step is
/// an identity for non-fahrenheit-compat builds and is not part of the choice).
#include <gtest/gtest.h>

namespace {

// Mirror of the remote-vs-probe selection in CN105Climate::setCurrentTemperature().
float selectCurrent(bool useRemoteAsCurrent, float remoteTemp, float probeTemp) {
    if (useRemoteAsCurrent && remoteTemp > 0.0f) {
        return remoteTemp;
    }
    return probeTemp;
}

}  // namespace

TEST(RemoteTempAsCurrentTest, RemoteShownWhenActiveAndEnabled) {
    // Room sensor 23.5 °C, unit probe 20.0 °C -> card should show the room value.
    EXPECT_FLOAT_EQ(selectCurrent(/*enabled=*/true, /*remote=*/23.5f, /*probe=*/20.0f), 23.5f);
}

TEST(RemoteTempAsCurrentTest, ProbeShownWhenDisabled) {
    // Default (opt-out): unchanged behavior, show the unit probe.
    EXPECT_FLOAT_EQ(selectCurrent(/*enabled=*/false, 23.5f, 20.0f), 20.0f);
}

TEST(RemoteTempAsCurrentTest, ProbeShownWhenRemoteTimedOut) {
    // remoteTemperature_ == 0 means remote temp reverted/timed out -> fall back to probe.
    EXPECT_FLOAT_EQ(selectCurrent(/*enabled=*/true, /*remote=*/0.0f, /*probe=*/20.0f), 20.0f);
}

TEST(RemoteTempAsCurrentTest, ProbeShownWhenRemoteNonPositive) {
    EXPECT_FLOAT_EQ(selectCurrent(/*enabled=*/true, /*remote=*/-1.0f, /*probe=*/20.0f), 20.0f);
}
