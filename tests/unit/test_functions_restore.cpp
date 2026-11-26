/// test_functions_restore.cpp — Tests for esphome::cn105::FunctionsSnapshot (production code).
/// Deps: functions_restore.h, cn105_protocol.h, cn105_types.h
///
/// Reading or writing Function Settings can switch some units off (#784). FunctionsSnapshot
/// saves the state before the access and decides, from each settings reply, whether to
/// restore it. Time is passed in, so every limit is tested with explicit timestamps.
#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include "functions_restore.h"

using esphome::cn105::FunctionsSnapshot;

namespace {

constexpr uint32_t kStartMs = 10000;
constexpr uint32_t kWindowMs = FUNCTIONS_RESTORE_WINDOW_MS;       // phase 2 limit
constexpr uint32_t kMaxAgeMs = FUNCTIONS_SNAPSHOT_MAX_AGE_MS;     // phase 1 limit

heatpumpSettings unitOn() {
    heatpumpSettings s{};
    s.power = "ON";
    s.mode = "HEAT";
    s.fan = "2";
    s.vane = "↓";
    s.wideVane = "|";
    s.temperature = 21.5f;
    return s;
}

// What the unit reports after switching itself off: same fields, power OFF.
heatpumpSettings unitOff() {
    heatpumpSettings s = unitOn();
    s.power = "OFF";
    return s;
}

// Snapshot with the read finished, window open at `at`.
FunctionsSnapshot armed(const heatpumpSettings& saved, uint32_t at = kStartMs) {
    FunctionsSnapshot snap;
    EXPECT_TRUE(snap.capture(saved, at));
    snap.open_window(at);
    return snap;
}

}  // namespace

// ════════════════════════════════════════════════════════════════
// capture()
// ════════════════════════════════════════════════════════════════

TEST(FunctionsSnapshotCapture, SavesStateWhenEnabled) {
    FunctionsSnapshot snap;
    EXPECT_TRUE(snap.capture(unitOn(), kStartMs));
    EXPECT_TRUE(snap.valid());
    EXPECT_FALSE(snap.window_open());
    EXPECT_STREQ(snap.saved().power, "ON");
    EXPECT_STREQ(snap.saved().mode, "HEAT");
}

TEST(FunctionsSnapshotCapture, DisabledSavesNothing) {
    FunctionsSnapshot snap;
    snap.set_enabled(false);
    EXPECT_FALSE(snap.capture(unitOn(), kStartMs));
    EXPECT_FALSE(snap.valid());
}

TEST(FunctionsSnapshotCapture, NullPowerOrModeSavesNothing) {
    FunctionsSnapshot snap;
    heatpumpSettings noPower = unitOn();
    noPower.power = nullptr;
    EXPECT_FALSE(snap.capture(noPower, kStartMs));
    EXPECT_FALSE(snap.valid());

    heatpumpSettings noMode = unitOn();
    noMode.mode = nullptr;
    EXPECT_FALSE(snap.capture(noMode, kStartMs));
    EXPECT_FALSE(snap.valid());
}

TEST(FunctionsSnapshotCapture, SecondCaptureKeepsTheFirstState) {
    // SET followed by GET: the state before the SET is the one to restore.
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), kStartMs));
    EXPECT_FALSE(snap.capture(unitOff(), kStartMs + 1000));
    EXPECT_STREQ(snap.saved().power, "ON");
}

TEST(FunctionsSnapshotCapture, SecondCaptureClosesTheWindowAndRefreshesTheTimer) {
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), kStartMs));
    snap.open_window(kStartMs + 100);
    ASSERT_TRUE(snap.window_open());

    EXPECT_FALSE(snap.capture(unitOn(), kStartMs + 2000));

    EXPECT_FALSE(snap.window_open());
    EXPECT_TRUE(snap.valid());
    // Phase 1 limit now counts from the second capture.
    EXPECT_FALSE(snap.expired(kStartMs + 2000 + kMaxAgeMs));
    EXPECT_TRUE(snap.expired(kStartMs + 2000 + kMaxAgeMs + 1));
}

TEST(FunctionsSnapshotCapture, ExpiredSnapshotIsReplaced) {
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOff(), kStartMs));
    EXPECT_TRUE(snap.capture(unitOn(), kStartMs + kMaxAgeMs + 1));
    EXPECT_STREQ(snap.saved().power, "ON");
}

TEST(FunctionsSnapshotCapture, ExpiredSnapshotIsDroppedWhenNewStateIsUnusable) {
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), kStartMs));
    heatpumpSettings noPower = unitOn();
    noPower.power = nullptr;
    EXPECT_FALSE(snap.capture(noPower, kStartMs + kMaxAgeMs + 1));
    EXPECT_FALSE(snap.valid());
}

// ════════════════════════════════════════════════════════════════
// Expiry and window
// ════════════════════════════════════════════════════════════════

TEST(FunctionsSnapshotExpiry, ReadInProgressUsesTheSnapshotMaxAge) {
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), kStartMs));
    EXPECT_FALSE(snap.expired(kStartMs + kMaxAgeMs));      // exactly at the limit is kept
    EXPECT_TRUE(snap.expired(kStartMs + kMaxAgeMs + 1));
}

TEST(FunctionsSnapshotExpiry, OpenWindowUsesTheRestoreWindow) {
    FunctionsSnapshot snap = armed(unitOn());
    EXPECT_FALSE(snap.expired(kStartMs + kWindowMs));
    EXPECT_TRUE(snap.expired(kStartMs + kWindowMs + 1));
}

TEST(FunctionsSnapshotExpiry, OpeningTheWindowRestartsTheTimer) {
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), kStartMs));
    snap.open_window(kStartMs + 8000);                      // read took 8 s
    EXPECT_FALSE(snap.expired(kStartMs + 8000 + kWindowMs));
    EXPECT_TRUE(snap.expired(kStartMs + 8000 + kWindowMs + 1));
}

TEST(FunctionsSnapshotExpiry, CustomWindowReplacesTheBaseWindow) {
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), kStartMs));
    snap.open_window(kStartMs, kWindowMs + 8000);           // e.g. an 8 s update interval
    EXPECT_FALSE(snap.expired(kStartMs + kWindowMs + 8000));
    EXPECT_TRUE(snap.expired(kStartMs + kWindowMs + 8000 + 1));
    EXPECT_TRUE(snap.on_settings(unitOff(), false, kStartMs + kWindowMs + 4000).restore);
}

TEST(FunctionsSnapshotExpiry, SurvivesMillisRollover) {
    const uint32_t start = UINT32_MAX - 1000;
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), start));
    EXPECT_FALSE(snap.expired(start + kMaxAgeMs));
    EXPECT_TRUE(snap.expired(start + kMaxAgeMs + 1));
}

TEST(FunctionsSnapshotExpiry, OpenWindowWithoutSnapshotDoesNothing) {
    FunctionsSnapshot snap;
    snap.open_window(kStartMs);
    EXPECT_FALSE(snap.valid());
    EXPECT_FALSE(snap.window_open());
}

TEST(FunctionsSnapshotExpiry, DiscardClearsSnapshotAndWindow) {
    FunctionsSnapshot snap = armed(unitOn());
    snap.discard();
    EXPECT_FALSE(snap.valid());
    EXPECT_FALSE(snap.window_open());
}

// ════════════════════════════════════════════════════════════════
// on_settings(): when nothing happens
// ════════════════════════════════════════════════════════════════

TEST(FunctionsSnapshotRestore, NoSnapshotDoesNothing) {
    FunctionsSnapshot snap;
    EXPECT_FALSE(snap.on_settings(unitOff(), false, kStartMs).restore);
}

TEST(FunctionsSnapshotRestore, ReadStillInProgressKeepsWaiting) {
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), kStartMs));
    EXPECT_FALSE(snap.on_settings(unitOff(), false, kStartMs + 1000).restore);
    EXPECT_TRUE(snap.valid());  // not discarded: the read has not finished
}

TEST(FunctionsSnapshotRestore, ExpiredSnapshotIsDiscardedWithoutRestoring) {
    FunctionsSnapshot snap = armed(unitOn());
    EXPECT_FALSE(snap.on_settings(unitOff(), false, kStartMs + kWindowMs + 1).restore);
    EXPECT_FALSE(snap.valid());
}

TEST(FunctionsSnapshotRestore, ReadThatNeverFinishedIsDiscarded) {
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), kStartMs));
    EXPECT_FALSE(snap.on_settings(unitOff(), false, kStartMs + kMaxAgeMs + 1).restore);
    EXPECT_FALSE(snap.valid());
}

TEST(FunctionsSnapshotRestore, PendingUserChangeDiscardsTheSnapshot) {
    FunctionsSnapshot snap = armed(unitOn());
    EXPECT_FALSE(snap.on_settings(unitOff(), /*user_change_pending=*/true, kStartMs + 100).restore);
    EXPECT_FALSE(snap.valid());
}

TEST(FunctionsSnapshotRestore, UnitThatWasOffIsNotRestored) {
    FunctionsSnapshot snap = armed(unitOff());
    EXPECT_FALSE(snap.on_settings(unitOff(), false, kStartMs + 100).restore);
    EXPECT_FALSE(snap.valid());
}

TEST(FunctionsSnapshotRestore, ReceivedNullPowerKeepsWatching) {
    // A settings reply without a power value says nothing: keep watching.
    FunctionsSnapshot snap = armed(unitOn());
    heatpumpSettings noPower = unitOn();
    noPower.power = nullptr;
    EXPECT_FALSE(snap.on_settings(noPower, false, kStartMs + 100).restore);
    EXPECT_TRUE(snap.valid());
}

TEST(FunctionsSnapshotRestore, UnitStillOnKeepsWatching) {
    FunctionsSnapshot snap = armed(unitOn());
    EXPECT_FALSE(snap.on_settings(unitOn(), false, kStartMs + 100).restore);
    EXPECT_TRUE(snap.valid());
    EXPECT_TRUE(snap.window_open());
}

TEST(FunctionsSnapshotRestore, UnitStillOnAtTheEndOfTheWindowIsDiscarded) {
    FunctionsSnapshot snap = armed(unitOn());
    EXPECT_FALSE(snap.on_settings(unitOn(), false, kStartMs + kWindowMs).restore);
    EXPECT_FALSE(snap.on_settings(unitOn(), false, kStartMs + kWindowMs + 1).restore);
    EXPECT_FALSE(snap.valid());
}

// ════════════════════════════════════════════════════════════════
// on_settings(): restoring
// ════════════════════════════════════════════════════════════════

TEST(FunctionsSnapshotRestore, PowerOffInsideTheWindowRestoresPowerOn) {
    FunctionsSnapshot snap = armed(unitOn());
    const auto r = snap.on_settings(unitOff(), false, kStartMs + 100);
    ASSERT_TRUE(r.restore);
    EXPECT_STREQ(r.power, "ON");
}

TEST(FunctionsSnapshotRestore, PowerOffAtTheLastMomentOfTheWindowRestores) {
    FunctionsSnapshot snap = armed(unitOn());
    EXPECT_TRUE(snap.on_settings(unitOff(), false, kStartMs + kWindowMs).restore);
}

TEST(FunctionsSnapshotRestore, UnchangedOptionalFieldsAreNotResent) {
    FunctionsSnapshot snap = armed(unitOn());
    const auto r = snap.on_settings(unitOff(), false, kStartMs + 100);
    ASSERT_TRUE(r.restore);
    EXPECT_STREQ(r.mode, "HEAT");  // the mode is always re-sent with the power
    EXPECT_EQ(r.fan, nullptr);
    EXPECT_EQ(r.vane, nullptr);
    EXPECT_EQ(r.wide_vane, nullptr);
    EXPECT_FALSE(r.set_temperature);
}

TEST(FunctionsSnapshotRestore, ChangedFieldsAreResent) {
    FunctionsSnapshot snap = armed(unitOn());
    heatpumpSettings received = unitOff();
    received.mode = "AUTO";
    received.fan = "AUTO";
    received.vane = "SWING";
    received.wideVane = "SWING";
    received.temperature = 24.0f;

    const auto r = snap.on_settings(received, false, kStartMs + 100);

    ASSERT_TRUE(r.restore);
    EXPECT_STREQ(r.mode, "HEAT");
    EXPECT_STREQ(r.fan, "2");
    EXPECT_STREQ(r.vane, "↓");
    EXPECT_STREQ(r.wide_vane, "|");
    ASSERT_TRUE(r.set_temperature);
    EXPECT_FLOAT_EQ(r.temperature, 21.5f);
}

TEST(FunctionsSnapshotRestore, FieldsCompareByContentNotByPointer) {
    FunctionsSnapshot snap = armed(unitOn());
    char fan[] = "2";   // same text, different pointer
    heatpumpSettings received = unitOff();
    received.fan = fan;
    const auto r = snap.on_settings(received, false, kStartMs + 100);
    ASSERT_TRUE(r.restore);
    EXPECT_EQ(r.fan, nullptr);
}

TEST(FunctionsSnapshotRestore, NullReceivedFieldCountsAsChanged) {
    FunctionsSnapshot snap = armed(unitOn());
    heatpumpSettings received = unitOff();
    received.mode = nullptr;
    received.fan = nullptr;
    received.vane = nullptr;
    received.wideVane = nullptr;
    const auto r = snap.on_settings(received, false, kStartMs + 100);
    ASSERT_TRUE(r.restore);
    EXPECT_STREQ(r.mode, "HEAT");
    EXPECT_STREQ(r.fan, "2");
    EXPECT_STREQ(r.vane, "↓");
    EXPECT_STREQ(r.wide_vane, "|");
}

TEST(FunctionsSnapshotRestore, NullSavedFieldIsNeverSent) {
    heatpumpSettings saved = unitOn();
    saved.fan = nullptr;
    saved.vane = nullptr;
    saved.wideVane = nullptr;
    FunctionsSnapshot snap = armed(saved);
    heatpumpSettings received = unitOff();
    received.fan = "AUTO";
    received.vane = "AUTO";
    received.wideVane = "SWING";
    const auto r = snap.on_settings(received, false, kStartMs + 100);
    ASSERT_TRUE(r.restore);
    EXPECT_EQ(r.fan, nullptr);
    EXPECT_EQ(r.vane, nullptr);
    EXPECT_EQ(r.wide_vane, nullptr);
}

TEST(FunctionsSnapshotRestore, TemperatureResentWhenReceivedIsNaN) {
    FunctionsSnapshot snap = armed(unitOn());
    heatpumpSettings received = unitOff();
    received.temperature = NAN;
    const auto r = snap.on_settings(received, false, kStartMs + 100);
    ASSERT_TRUE(r.restore);
    EXPECT_TRUE(r.set_temperature);
    EXPECT_FLOAT_EQ(r.temperature, 21.5f);
}

TEST(FunctionsSnapshotRestore, TemperatureWithinToleranceIsNotResent) {
    FunctionsSnapshot snap = armed(unitOn());
    heatpumpSettings received = unitOff();
    received.temperature = 21.505f;   // within 0.01
    EXPECT_FALSE(snap.on_settings(received, false, kStartMs + 100).set_temperature);
}

TEST(FunctionsSnapshotRestore, UnusableSavedTemperatureIsNeverSent) {
    for (float saved_temp : {NAN, -1.0f, 0.0f}) {
        heatpumpSettings saved = unitOn();
        saved.temperature = saved_temp;
        FunctionsSnapshot snap = armed(saved);
        heatpumpSettings received = unitOff();
        received.temperature = 24.0f;
        const auto r = snap.on_settings(received, false, kStartMs + 100);
        ASSERT_TRUE(r.restore);
        EXPECT_FALSE(r.set_temperature) << saved_temp;
    }
}

TEST(FunctionsSnapshotRestore, RestoreHappensOnlyOnce) {
    FunctionsSnapshot snap = armed(unitOn());
    ASSERT_TRUE(snap.on_settings(unitOff(), false, kStartMs + 100).restore);
    EXPECT_FALSE(snap.valid());
    EXPECT_FALSE(snap.on_settings(unitOff(), false, kStartMs + 200).restore);
}

// ════════════════════════════════════════════════════════════════
// Whole sequences
// ════════════════════════════════════════════════════════════════

TEST(FunctionsSnapshotSequence, WriteThenReadBackRestoresTheStateBeforeTheWrite) {
    FunctionsSnapshot snap;
    ASSERT_TRUE(snap.capture(unitOn(), kStartMs));                 // setFunctions()
    // The write powers the unit off; the settings reply arrives before the read-back starts.
    EXPECT_FALSE(snap.on_settings(unitOff(), false, kStartMs + 300).restore);  // window not open yet
    EXPECT_FALSE(snap.capture(unitOff(), kStartMs + 600));         // GET part 1 after the ack
    snap.open_window(kStartMs + 900);                              // 0x22 arrives
    const auto r = snap.on_settings(unitOff(), false, kStartMs + 1200);
    ASSERT_TRUE(r.restore);
    EXPECT_STREQ(r.power, "ON");
}

TEST(FunctionsSnapshotSequence, UserChangeAfterTheReadWinsOverTheRestore) {
    FunctionsSnapshot snap = armed(unitOn());
    snap.discard();                                                // finalizeControlIfUpdated()
    EXPECT_FALSE(snap.on_settings(unitOff(), false, kStartMs + 100).restore);
}

TEST(FunctionsSnapshotSequence, DisabledOptionNeverRestores) {
    FunctionsSnapshot snap;
    snap.set_enabled(false);
    snap.capture(unitOn(), kStartMs);
    snap.open_window(kStartMs);
    EXPECT_FALSE(snap.on_settings(unitOff(), false, kStartMs + 100).restore);
}

TEST(FunctionsSnapshotSequence, CanBeReusedAfterARestore) {
    FunctionsSnapshot snap = armed(unitOn());
    ASSERT_TRUE(snap.on_settings(unitOff(), false, kStartMs + 100).restore);
    const uint32_t later = kStartMs + 60000;
    ASSERT_TRUE(snap.capture(unitOn(), later));
    snap.open_window(later);
    EXPECT_TRUE(snap.on_settings(unitOff(), false, later + 100).restore);
}
