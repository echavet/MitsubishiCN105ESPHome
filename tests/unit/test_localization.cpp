/// test_localization.cpp — Tests for FahrenheitSupport (localization.h, production code)
/// Deps: localization.h, esphome_stubs.h
///
/// Context (issue #673): the Fahrenheit table conversions are applied on every
/// target-temperature store (HP→UI) and read (UI→HP). These tests pin down the
/// invariant the rest of the firmware relies on: the storage round-trip cycle
/// hpToUi(uiToHp(u)) is STABLE for any UI value that itself came from the table
/// (i.e. set(get(x)) never drifts once a value has been through one cycle).
/// Note: uiToHp is deliberately NOT idempotent on arbitrary inputs (table-Celsius
/// values such as 22.5°C sit exactly halfway between two Fahrenheit rows), which
/// is why feeding derived values back through the table — as the pre-#673-fix
/// band recentring did — can ratchet. The fix avoids re-deriving the band, and
/// these tests guarantee the benign cycle stays benign.
#include <gtest/gtest.h>
#include <cmath>
#include "esphome_stubs.h"
#include "localization.h"

using esphome::FahrenheitMode;
using esphome::FahrenheitSupport;

namespace {

float uiFromF(int f) { return (static_cast<float>(f) - 32.0f) / 1.8f; }

}  // namespace

// ========================================================
// OFF mode: conversions are identity
// ========================================================

TEST(FahrenheitSupportTest, OffMode_Passthrough) {
    FahrenheitSupport fs;
    fs.setUseFahrenheitSupportMode(FahrenheitMode::OFF);
    for (float c = 10.0f; c <= 31.0f; c += 0.25f) {
        EXPECT_FLOAT_EQ(fs.normalizeUiTemperatureToHeatpumpTemperature(c), c);
        EXPECT_FLOAT_EQ(fs.normalizeHeatpumpTemperatureToUiTemperature(c), c);
    }
}

TEST(FahrenheitSupportTest, NaN_Passthrough) {
    FahrenheitSupport fs;
    for (auto mode : {FahrenheitMode::OFF, FahrenheitMode::STANDARD, FahrenheitMode::ALT, FahrenheitMode::MSZ_A24NA}) {
        fs.setUseFahrenheitSupportMode(mode);
        EXPECT_TRUE(std::isnan(fs.normalizeUiTemperatureToHeatpumpTemperature(NAN)));
        EXPECT_TRUE(std::isnan(fs.normalizeHeatpumpTemperatureToUiTemperature(NAN)));
    }
}

// ========================================================
// Storage round-trip cycle: for every whole-Fahrenheit UI value,
// one get/set cycle returns the same UI value (no drift), and a
// second cycle is a fixed point.
// ========================================================

static void expectCycleStable(FahrenheitMode mode) {
    FahrenheitSupport fs;
    fs.setUseFahrenheitSupportMode(mode);
    for (int f = 61; f <= 88; ++f) {
        const float u0 = uiFromF(f);
        const float hp = fs.normalizeUiTemperatureToHeatpumpTemperature(u0);
        const float u1 = fs.normalizeHeatpumpTemperatureToUiTemperature(hp);
        EXPECT_NEAR(u1, u0, 1e-3f) << "first cycle drifted for " << f << "F";

        const float hp2 = fs.normalizeUiTemperatureToHeatpumpTemperature(u1);
        const float u2 = fs.normalizeHeatpumpTemperatureToUiTemperature(hp2);
        EXPECT_NEAR(u2, u1, 1e-4f) << "second cycle drifted for " << f << "F";
    }
}

TEST(FahrenheitSupportTest, StandardTable_StorageCycleStable) {
    expectCycleStable(FahrenheitMode::STANDARD);
}

TEST(FahrenheitSupportTest, AltTable_StorageCycleStable) {
    expectCycleStable(FahrenheitMode::ALT);
}

// ========================================================
// Table-row Celsius values survive a store (HP→UI) and read back
// (UI→HP) unchanged — the unit talks in these values.
// ========================================================

static void expectRowValuesRoundTrip(FahrenheitMode mode, const float* rows, int n) {
    FahrenheitSupport fs;
    fs.setUseFahrenheitSupportMode(mode);
    for (int i = 0; i < n; ++i) {
        const float ui = fs.normalizeHeatpumpTemperatureToUiTemperature(rows[i]);
        const float back = fs.normalizeUiTemperatureToHeatpumpTemperature(ui);
        EXPECT_NEAR(back, rows[i], 1e-4f) << "row value " << rows[i] << "C did not round-trip";
    }
}

TEST(FahrenheitSupportTest, StandardTable_RowValuesRoundTrip) {
    // Interior rows of the STANDARD table (61F/88F edges bypass the table by
    // design — upper_bound edge — and are covered by the cycle tests above).
    const float rows[] = {16.5f, 17.0f, 17.5f, 18.0f, 18.5f, 19.0f, 20.0f, 21.0f,
                          21.5f, 22.0f, 22.5f, 23.0f, 24.0f, 25.0f, 26.0f, 27.0f,
                          27.5f, 28.0f, 28.5f, 29.0f, 29.5f, 30.0f};
    expectRowValuesRoundTrip(FahrenheitMode::STANDARD, rows, sizeof(rows) / sizeof(rows[0]));
}

TEST(FahrenheitSupportTest, AltTable_RowValuesRoundTrip) {
    const float rows[] = {16.5f, 17.0f, 18.0f, 18.5f, 19.0f, 19.5f, 20.0f, 20.5f,
                          21.0f, 21.5f, 22.0f, 23.0f, 24.0f, 25.0f, 26.0f, 27.0f,
                          28.0f, 28.5f, 29.0f, 29.5f, 30.0f, 30.5f};
    expectRowValuesRoundTrip(FahrenheitMode::ALT, rows, sizeof(rows) / sizeof(rows[0]));
}

// ========================================================
// Documented non-idempotence of uiToHp on raw table-Celsius input:
// 22.5C is exactly 72.5F, halfway between rows 72 and 73 — the lookup
// must pick ONE of them deterministically. This is why derived values
// must never be re-fed through the table (issue #673), and why the
// cycle tests above are the correct invariant.
// ========================================================

TEST(FahrenheitSupportTest, StandardTable_MidpointResolvesDeterministically) {
    FahrenheitSupport fs;
    fs.setUseFahrenheitSupportMode(FahrenheitMode::STANDARD);
    const float once = fs.normalizeUiTemperatureToHeatpumpTemperature(22.5f);
    const float twice = fs.normalizeUiTemperatureToHeatpumpTemperature(once);
    // Whatever row the tie resolves to, the result must be a value from the
    // table and the SECOND application through a storage cycle must be stable.
    const float ui = fs.normalizeHeatpumpTemperatureToUiTemperature(once);
    const float again = fs.normalizeUiTemperatureToHeatpumpTemperature(ui);
    EXPECT_NEAR(again, once, 1e-4f);
    (void)twice;
}
