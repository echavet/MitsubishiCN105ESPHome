/// test_connect_handshake.cpp — Tests for the CN105 connection sequence
/// (standard 0x5A, optional installer upgrade 0x5B).
/// Deps: connect_handshake.h, cn105_protocol.h, frame_parser.h, cn105_types.h (production code)
///
/// Pattern: the handshake state, timers and decisions live in the production class
/// ConnectHandshake (connect_handshake.h), which CN105Climate drives.
/// These tests drive that same object through a fake of its hooks: a fake clock,
/// ESPHome-style named timeouts (set_timeout replaces a timer with the same name,
/// cancel_timeout removes it) and the connected/uptime bookkeeping of
/// setHeatpumpConnected(). The only driver code mirrored here is the part outside
/// the handshake class:
///   - hp_writings.cpp: sendFirstConnectionPacket() (disconnect, write 0x5A, on_connect_sent())
///   - cn105.cpp:       reconnectUART() (on_disconnect(), then sendFirstConnectionPacket())
///   - cn105.cpp:       setHeatpumpConnected()
/// The wire frames (INST_CONNECT / CONNECT) are tested against the real
/// FrameParser and checksum.
#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "connect_handshake.h"
#include "cn105_protocol.h"
#include "frame_parser.h"
#include "cn105_types.h"

namespace {

enum class State { CONNECTING, CONNECTED, DISCONNECTED };

using esphome::cn105::ConnectHandshake;
using esphome::cn105::HandshakeEvent;
using esphome::cn105::HandshakePhase;
using esphome::cn105::HandshakeProtocol;
constexpr uint32_t kFirstConnectTimeoutMs = HandshakeProtocol::kFirstConnectTimeoutMs;          // "checkFirstConnection"
constexpr uint32_t kInstallerConnectTimeoutMs = HandshakeProtocol::kInstallerConnectTimeoutMs;  // "checkInstallerConnection"

class HandshakeSim {
public:
    // Timer callbacks capture `this`, so the object must never move or be copied.
    explicit HandshakeSim(bool installer_mode) {
        hs.set_installer_mode(installer_mode);
    }
    HandshakeSim(const HandshakeSim&) = delete;
    HandshakeSim& operator=(const HandshakeSim&) = delete;
    HandshakeSim(HandshakeSim&&) = delete;
    HandshakeSim& operator=(HandshakeSim&&) = delete;

    // ── observable state ──
    State state = State::CONNECTING;  // setupUART() leaves the FSM in CONNECTING
    int uptime_starts = 0;            // hp_uptime_connection_sensor_->start() calls
    int reconnects = 0;               // reconnectUART() calls
    std::vector<uint8_t> sent;        // command byte (offset 1) of each frame sent
    std::vector<HandshakeEvent> events;
    std::vector<bool> connected_at_event;  // connected() when each event fired
    bool fail_installer_write_once = false;  // next 0x5B write reconnects re-entrantly

    // The production handshake under test, wired to this fake.
    ConnectHandshake hs{esphome::cn105::HandshakeHooks{
        [this]() {  // sendInstallerConnectPacket(): 0x5B
            sent.push_back(INST_CONNECT[1]);
            if (fail_installer_write_once) {  // writePacket() failure calls reconnectUART()
                fail_installer_write_once = false;
                reconnectUART();
            }
        },
        [this](const char* name, uint32_t delay, std::function<void()> cb) {
            set_timeout(name, delay, std::move(cb));
        },
        [this](const char* name) { cancel_timeout(name); },
        [this]() { setHeatpumpConnected(true); },
        [this]() { reconnectUART(); },
        [this](HandshakeEvent event) {
            events.push_back(event);
            connected_at_event.push_back(connected());
        },
    }};

    bool connected() const { return state == State::CONNECTED; }

    // Mirror of CN105Climate::sendFirstConnectionPacket()
    void sendFirstConnectionPacket() {
        setHeatpumpConnected(false);
        sent.push_back(CONNECT[1]);  // 0x5A
        hs.on_connect_sent();
    }

    // CN105Climate::processCommand() cases 0x7a / 0x7b (frames accepted by the profile)
    void onFrame(uint8_t command) {
        switch (command) {
        case 0x7a: hs.on_standard_reply(); break;
        case 0x7b: hs.on_installer_reply(); break;
        default: break;
        }
    }

    // Advance the fake clock, firing due timers in deadline order.
    void advance(uint32_t ms) {
        const uint32_t target = now_ + ms;
        for (;;) {
            int best = -1;
            for (size_t i = 0; i < timers_.size(); ++i) {
                if (timers_[i].deadline <= target &&
                    (best < 0 || timers_[i].deadline < timers_[best].deadline)) {
                    best = static_cast<int>(i);
                }
            }
            if (best < 0) break;
            Timer t = timers_[best];
            timers_.erase(timers_.begin() + best);
            now_ = t.deadline;
            t.cb();
        }
        now_ = target;
    }

private:
    struct Timer {
        std::string name;
        uint32_t deadline;
        std::function<void()> cb;
    };

    void set_timeout(const std::string& name, uint32_t delay, std::function<void()> cb) {
        cancel_timeout(name);
        timers_.push_back({name, now_ + delay, std::move(cb)});
    }

    void cancel_timeout(const std::string& name) {
        for (size_t i = 0; i < timers_.size(); ++i) {
            if (timers_[i].name == name) {
                timers_.erase(timers_.begin() + i);
                return;
            }
        }
    }

    // Mirror of CN105Climate::reconnectUART(): disconnectUART() then CONNECT again.
    void reconnectUART() {
        ++reconnects;
        setHeatpumpConnected(false);
        hs.on_disconnect();
        sendFirstConnectionPacket();
    }

    // Mirror of CN105Climate::setHeatpumpConnected()
    void setHeatpumpConnected(bool connected_now) {
        if (connected_now) {
            state = State::CONNECTED;
            ++uptime_starts;
        } else {
            if (state == State::CONNECTED) state = State::DISCONNECTED;
        }
    }

    uint32_t now_ = 0;
    std::vector<Timer> timers_;
};

}  // namespace

// ════════════════════════════════════════════════════════════════
// Wire format — real production constants through the real parser
// ════════════════════════════════════════════════════════════════

TEST(ConnectHandshakeFrames, StandardConnectIsValidFrame) {
    cn105_protocol::FrameParser parser;
    for (int i = 0; i < CONNECT_LEN; ++i) parser.feed(CONNECT[i]);
    ASSERT_TRUE(parser.frame_complete());
    EXPECT_TRUE(parser.checksum_valid());
    EXPECT_EQ(parser.command(), 0x5A);
}

TEST(ConnectHandshakeFrames, InstallerConnectIsValidFrame) {
    cn105_protocol::FrameParser parser;
    for (int i = 0; i < INST_CONNECT_LEN; ++i) parser.feed(INST_CONNECT[i]);
    ASSERT_TRUE(parser.frame_complete());
    EXPECT_TRUE(parser.checksum_valid());
    EXPECT_EQ(parser.command(), 0x5B);
    EXPECT_EQ(parser.data_length(), 1);
    EXPECT_EQ(parser.data()[0], 0xC9);
}

TEST(ConnectHandshakeFrames, ReplyFramesParse) {
    // 0x7A: FC 7A 01 30 01 00 54 (real capture). 0x7B uses the same shape.
    cn105_protocol::FrameParser parser;
    const uint8_t ack_7a[] = {0xFC, 0x7A, 0x01, 0x30, 0x01, 0x00, 0x54};
    for (uint8_t b : ack_7a) parser.feed(b);
    ASSERT_TRUE(parser.frame_complete());
    EXPECT_TRUE(parser.checksum_valid());
    EXPECT_EQ(parser.command(), 0x7A);

    parser.reset();
    uint8_t ack_7b[] = {0xFC, 0x7B, 0x01, 0x30, 0x01, 0x00, 0x00};
    ack_7b[6] = cn105_protocol::checksum(ack_7b, 6);
    for (uint8_t b : ack_7b) parser.feed(b);
    ASSERT_TRUE(parser.frame_complete());
    EXPECT_TRUE(parser.checksum_valid());
    EXPECT_EQ(parser.command(), 0x7B);
}

// ════════════════════════════════════════════════════════════════
// Standard mode (installer_mode: false)
// ════════════════════════════════════════════════════════════════

TEST(ConnectHandshake, Standard_ConnectsOn7A_WithoutSending5B) {
    HandshakeSim sim(false);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    EXPECT_FALSE(sim.connected());

    sim.onFrame(0x7a);

    EXPECT_TRUE(sim.connected());
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A}));
    EXPECT_EQ(sim.hs.phase(), HandshakePhase::DONE);
    EXPECT_EQ(sim.uptime_starts, 1);
}

TEST(ConnectHandshake, Standard_7ACancelsFirstConnectTimeout) {
    HandshakeSim sim(false);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);

    sim.advance(60000);

    EXPECT_TRUE(sim.connected());
    EXPECT_EQ(sim.reconnects, 0);
}

TEST(ConnectHandshake, Standard_Unsolicited7BIsIgnored) {
    HandshakeSim sim(false);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);

    sim.onFrame(0x7b);

    EXPECT_FALSE(sim.hs.installer_session_active());
    EXPECT_EQ(sim.uptime_starts, 1);
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A}));
}

// ════════════════════════════════════════════════════════════════
// Installer mode (installer_mode: true) — happy path
// ════════════════════════════════════════════════════════════════

TEST(ConnectHandshake, Installer_7ATriggers5B_AndDefersConnected) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver

    sim.onFrame(0x7a);

    // Upgrade is in flight: 0x5B sent, poll cycle must NOT start yet.
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A, 0x5B}));
    EXPECT_EQ(sim.hs.phase(), HandshakePhase::AWAIT_INSTALLER);
    EXPECT_FALSE(sim.connected());
    EXPECT_FALSE(sim.hs.installer_session_active());
}

TEST(ConnectHandshake, Installer_7BCompletesSession) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);

    sim.onFrame(0x7b);

    EXPECT_TRUE(sim.connected());
    EXPECT_TRUE(sim.hs.installer_session_active());
    EXPECT_EQ(sim.uptime_starts, 1);
}

TEST(ConnectHandshake, Installer_7BCancelsFallbackTimeout) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.advance(3000);
    sim.onFrame(0x7b);

    sim.advance(60000);

    EXPECT_TRUE(sim.connected());
    EXPECT_EQ(sim.uptime_starts, 1);  // the cancelled timer did not connect a second time
    EXPECT_EQ(sim.reconnects, 0);
}

TEST(ConnectHandshake, Installer_5AAlwaysPrecedes5B) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.onFrame(0x7b);

    ASSERT_EQ(sim.sent.size(), 2u);
    EXPECT_EQ(sim.sent[0], 0x5A);
    EXPECT_EQ(sim.sent[1], 0x5B);
}

// ════════════════════════════════════════════════════════════════
// Installer mode — unit ignores 0x5B (fallback to standard session)
// ════════════════════════════════════════════════════════════════

TEST(ConnectHandshake, Installer_No7B_FallsBackToStandardAfterTimeout) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);

    sim.advance(kInstallerConnectTimeoutMs - 1);
    EXPECT_FALSE(sim.connected());

    sim.advance(1);
    EXPECT_TRUE(sim.connected());
    EXPECT_FALSE(sim.hs.installer_session_active());
    EXPECT_EQ(sim.uptime_starts, 1);
    EXPECT_EQ(sim.reconnects, 0);  // fallback must not tear the link down
}

TEST(ConnectHandshake, Installer_Late7BAfterFallback_IsIgnored) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.advance(kInstallerConnectTimeoutMs);
    ASSERT_TRUE(sim.connected());
    ASSERT_EQ(sim.uptime_starts, 1);

    sim.onFrame(0x7b);  // stray: no 0x5B pending any more

    EXPECT_TRUE(sim.connected());
    EXPECT_FALSE(sim.hs.installer_session_active());  // session stays standard
    EXPECT_TRUE(sim.hs.installer_declined());         // still declined until reboot
    EXPECT_EQ(sim.uptime_starts, 1);                  // uptime counter not reset
}

TEST(ConnectHandshake, Installer_Stray7AWhileAwaiting7B_IsIgnored) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);                // reply to 0x5A -> 0x5B goes out

    sim.onFrame(0x7a);                // stray: only 0x7B answers 0x5B

    EXPECT_FALSE(sim.connected());    // still waiting for 0x7B
    EXPECT_EQ(sim.hs.phase(), HandshakePhase::AWAIT_INSTALLER);
    EXPECT_FALSE(sim.hs.installer_declined());
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A, 0x5B}));  // no second 0x5B
    EXPECT_EQ(sim.uptime_starts, 0);

    sim.onFrame(0x7b);                // the real reply still completes the upgrade
    EXPECT_TRUE(sim.connected());
    EXPECT_TRUE(sim.hs.installer_session_active());
    EXPECT_EQ(sim.uptime_starts, 1);
}

TEST(ConnectHandshake, Installer_Stray7AWhileAwaiting7B_TimeoutStillFallsBack) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.advance(1000);
    sim.onFrame(0x7a);                // stray: does not cut the upgrade window short

    sim.advance(kInstallerConnectTimeoutMs - 1000 - 1);
    EXPECT_FALSE(sim.connected());

    sim.advance(1);
    EXPECT_TRUE(sim.connected());
    EXPECT_TRUE(sim.hs.installer_declined());
    EXPECT_EQ(sim.uptime_starts, 1);
    EXPECT_EQ(sim.reconnects, 0);
}

TEST(ConnectHandshake, Unsolicited7AWhileConnected_IsIgnored) {
    for (bool installer_mode : {false, true}) {
        HandshakeSim sim(installer_mode);
        sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
        sim.onFrame(0x7a);
        if (installer_mode) sim.onFrame(0x7b);
        ASSERT_TRUE(sim.connected());
        const auto sent_before = sim.sent;

        sim.onFrame(0x7a);  // stray reply mid-session

        EXPECT_TRUE(sim.connected());
        EXPECT_EQ(sim.sent, sent_before);  // no 0x5B injected into the poll cycle
        EXPECT_EQ(sim.uptime_starts, 1);
    }
}

TEST(ConnectHandshake, Installer_TimeoutFallback_SkipsInstallerOnReconnect) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.advance(kInstallerConnectTimeoutMs);  // no 0x7B: fall back
    ASSERT_TRUE(sim.connected());

    sim.sendFirstConnectionPacket();  // reconnect (e.g. reconnectUART())
    sim.onFrame(0x7a);

    EXPECT_TRUE(sim.connected());  // connects at once, no 5 s wait
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A, 0x5B, 0x5A}));
    EXPECT_EQ(sim.hs.phase(), HandshakePhase::DONE);
}

TEST(ConnectHandshake, Installer_Late7BAfterFallback_StillSkipsInstallerOnReconnect) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.advance(kInstallerConnectTimeoutMs);
    sim.onFrame(0x7b);  // stray: does not lift the decline

    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);

    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A, 0x5B, 0x5A}));
    EXPECT_TRUE(sim.connected());
}

TEST(ConnectHandshake, Stray7ABeforeConnectSent_IsIgnored) {
    HandshakeSim sim(false);  // no CONNECT written yet (phase IDLE)

    sim.onFrame(0x7a);

    EXPECT_FALSE(sim.connected());
    EXPECT_EQ(sim.hs.phase(), HandshakePhase::IDLE);
    EXPECT_EQ(sim.uptime_starts, 0);
}

TEST(ConnectHandshake, Installer_Unsolicited7BBefore7A_IsIgnored) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver

    sim.onFrame(0x7b);

    EXPECT_FALSE(sim.connected());
    EXPECT_FALSE(sim.hs.installer_session_active());
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A}));
}

// ════════════════════════════════════════════════════════════════
// No reply at all / reconnects
// ════════════════════════════════════════════════════════════════

TEST(ConnectHandshake, No7A_ReconnectsAndResends5A) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver

    sim.advance(kFirstConnectTimeoutMs);

    EXPECT_EQ(sim.reconnects, 1);
    EXPECT_FALSE(sim.connected());
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A, 0x5A}));  // never jumps straight to 0x5B
}

TEST(ConnectHandshake, No7A_KeepsRetryingEveryTimeout) {
    HandshakeSim sim(false);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver

    sim.advance(3 * kFirstConnectTimeoutMs);

    EXPECT_EQ(sim.reconnects, 3);
    EXPECT_FALSE(sim.connected());
}

TEST(ConnectHandshake, ReconnectDuringUpgradeWindow_CancelsStaleInstallerTimeout) {
    constexpr uint32_t kUpgradeAt = 1000;    // 0x7A arrives, 0x5B goes out
    constexpr uint32_t kReconnectAt = 3000;  // fresh handshake starts
    constexpr uint32_t kOldDeadline = kUpgradeAt + kInstallerConnectTimeoutMs;
    constexpr uint32_t kNewDeadline = kReconnectAt + kFirstConnectTimeoutMs;
    constexpr uint32_t kCheckAt = kOldDeadline + 1000;
    static_assert(kCheckAt > kOldDeadline && kCheckAt < kNewDeadline,
                  "check time must fall after the stale deadline and before the new one");

    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.advance(kUpgradeAt);
    sim.onFrame(0x7a);                // 0x5B sent, fallback timer due at kOldDeadline
    sim.advance(kReconnectAt - kUpgradeAt);
    sim.sendFirstConnectionPacket();

    // Run past the OLD fallback deadline but before the new checkFirstConnection one.
    sim.advance(kCheckAt - kReconnectAt);

    EXPECT_FALSE(sim.connected()) << "stale checkInstallerConnection fired after reconnect";
    EXPECT_EQ(sim.uptime_starts, 0);
}

TEST(ConnectHandshake, ReconnectWhileSending5B_RestartsHandshakeCleanly) {
    HandshakeSim sim(true);
    sim.fail_installer_write_once = true;
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);  // 0x5B write fails -> reconnectUART() -> 0x5A resent

    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A, 0x5B, 0x5A}));
    EXPECT_EQ(sim.hs.phase(), HandshakePhase::AWAIT_STANDARD);

    sim.advance(kInstallerConnectTimeoutMs);  // timer armed during the failed 0x5B write
    EXPECT_FALSE(sim.connected()) << "stale installer timeout connected without a 0x7A";
    EXPECT_FALSE(sim.hs.installer_declined());

    sim.onFrame(0x7a);  // reply to the new 0x5A
    sim.onFrame(0x7b);
    EXPECT_TRUE(sim.connected());
    EXPECT_TRUE(sim.hs.installer_session_active());
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A, 0x5B, 0x5A, 0x5B}));
    EXPECT_EQ(sim.reconnects, 1);
    EXPECT_EQ(sim.uptime_starts, 1);
}

TEST(ConnectHandshake, ReconnectResetsInstallerFlags) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.onFrame(0x7b);
    ASSERT_TRUE(sim.hs.installer_session_active());

    sim.sendFirstConnectionPacket();

    EXPECT_EQ(sim.hs.phase(), HandshakePhase::AWAIT_STANDARD);
    EXPECT_FALSE(sim.hs.installer_session_active());
    EXPECT_EQ(sim.state, State::DISCONNECTED);
}

TEST(ConnectHandshake, FullInstallerSessionCanBeRepeatedAfterReconnect) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.onFrame(0x7b);

    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);
    EXPECT_FALSE(sim.connected());  // waits for 0x7B again
    sim.onFrame(0x7b);

    EXPECT_TRUE(sim.connected());
    EXPECT_TRUE(sim.hs.installer_session_active());
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A, 0x5B, 0x5A, 0x5B}));
    EXPECT_EQ(sim.uptime_starts, 2);
}

// ════════════════════════════════════════════════════════════════
// Events reported to the driver (logging, cache reset)
// ════════════════════════════════════════════════════════════════

TEST(ConnectHandshakeEvents, Standard_SessionStartsBeforeConnected) {
    HandshakeSim sim(false);
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);

    EXPECT_EQ(sim.events, (std::vector<HandshakeEvent>{HandshakeEvent::SESSION_STARTED}));
    EXPECT_EQ(sim.connected_at_event, (std::vector<bool>{false}));  // caches reset before polling
    EXPECT_TRUE(sim.connected());
}

TEST(ConnectHandshakeEvents, Installer_HappyPath) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);
    sim.onFrame(0x7b);

    EXPECT_EQ(sim.events, (std::vector<HandshakeEvent>{
        HandshakeEvent::SESSION_STARTED, HandshakeEvent::INSTALLER_ACTIVE}));
}

TEST(ConnectHandshakeEvents, Installer_Timeout) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);
    sim.advance(kInstallerConnectTimeoutMs);

    EXPECT_EQ(sim.events, (std::vector<HandshakeEvent>{
        HandshakeEvent::SESSION_STARTED, HandshakeEvent::INSTALLER_TIMEOUT}));
    EXPECT_TRUE(sim.hs.installer_declined());
}

TEST(ConnectHandshakeEvents, Installer_TimeoutThenSkippedOnReconnect) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);
    sim.advance(kInstallerConnectTimeoutMs);
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);

    EXPECT_EQ(sim.events, (std::vector<HandshakeEvent>{
        HandshakeEvent::SESSION_STARTED, HandshakeEvent::INSTALLER_TIMEOUT,
        HandshakeEvent::SESSION_STARTED, HandshakeEvent::INSTALLER_SKIPPED}));
}

TEST(ConnectHandshakeEvents, UnexpectedRepliesAreReported) {
    HandshakeSim sim(false);
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7b);  // 0x5B never sent
    sim.onFrame(0x7a);
    sim.onFrame(0x7a);  // already connected

    EXPECT_EQ(sim.events, (std::vector<HandshakeEvent>{
        HandshakeEvent::UNEXPECTED_INSTALLER, HandshakeEvent::SESSION_STARTED,
        HandshakeEvent::UNEXPECTED_STANDARD}));
}

TEST(ConnectHandshakeEvents, Installer_StrayRepliesAreReported) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);
    sim.onFrame(0x7a);  // stray while waiting for 0x7B
    sim.advance(kInstallerConnectTimeoutMs);
    sim.onFrame(0x7b);  // late: after the fallback

    EXPECT_EQ(sim.events, (std::vector<HandshakeEvent>{
        HandshakeEvent::SESSION_STARTED, HandshakeEvent::UNEXPECTED_STANDARD,
        HandshakeEvent::INSTALLER_TIMEOUT, HandshakeEvent::UNEXPECTED_INSTALLER}));
}

TEST(ConnectHandshake, DisconnectCancelsBothTimers) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);  // installer timer armed, first timer cancelled
    sim.hs.on_disconnect();
    sim.sendFirstConnectionPacket();  // re-arms the first timer only
    sim.hs.on_disconnect();           // e.g. UART torn down before any reply

    sim.advance(60000);

    EXPECT_EQ(sim.reconnects, 0);  // first-connect timer did not fire
    EXPECT_FALSE(sim.connected()); // installer timer did not fire
    EXPECT_EQ(sim.hs.phase(), HandshakePhase::IDLE);
}

TEST(ConnectHandshake, DisconnectKeepsInstallerDeclined) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();
    sim.onFrame(0x7a);
    sim.advance(kInstallerConnectTimeoutMs);
    ASSERT_TRUE(sim.hs.installer_declined());

    sim.hs.on_disconnect();

    EXPECT_TRUE(sim.hs.installer_declined());  // lasts until reboot
}

// ════════════════════════════════════════════════════════════════
// Decision functions — direct truth tables (production code)
// ════════════════════════════════════════════════════════════════

namespace {
constexpr HandshakePhase kAllPhases[] = {HandshakePhase::IDLE, HandshakePhase::AWAIT_STANDARD,
                                         HandshakePhase::AWAIT_INSTALLER, HandshakePhase::DONE};

void ExpectNoAction(const HandshakeProtocol::Decision& d) {
    EXPECT_FALSE(d.begin_session);
    EXPECT_FALSE(d.send_installer);
    EXPECT_FALSE(d.cancel_installer_timeout);
    EXPECT_FALSE(d.session_active);
    EXPECT_FALSE(d.decline_installer);
    EXPECT_FALSE(d.mark_connected);
    EXPECT_FALSE(d.reconnect);
}
}  // namespace

TEST(HandshakeDecision, StandardReply_StandardMode_ConnectsImmediately) {
    auto d = HandshakeProtocol::standard_reply(HandshakePhase::AWAIT_STANDARD,
        /*installer_mode=*/false, /*installer_declined=*/false);
    EXPECT_TRUE(d.begin_session);
    EXPECT_TRUE(d.mark_connected);
    EXPECT_FALSE(d.send_installer);
    EXPECT_FALSE(d.session_active);
    EXPECT_FALSE(d.cancel_installer_timeout);
    EXPECT_FALSE(d.decline_installer);
}

TEST(HandshakeDecision, StandardReply_InstallerMode_UpgradesInsteadOfConnecting) {
    auto d = HandshakeProtocol::standard_reply(HandshakePhase::AWAIT_STANDARD,
        /*installer_mode=*/true, /*installer_declined=*/false);
    EXPECT_TRUE(d.begin_session);
    EXPECT_TRUE(d.send_installer);
    EXPECT_FALSE(d.mark_connected);
    EXPECT_FALSE(d.session_active);
}

TEST(HandshakeDecision, StandardReply_Declined_ConnectsWithoutUpgrade) {
    auto d = HandshakeProtocol::standard_reply(HandshakePhase::AWAIT_STANDARD,
        /*installer_mode=*/true, /*installer_declined=*/true);
    EXPECT_TRUE(d.begin_session);
    EXPECT_FALSE(d.send_installer);
    EXPECT_TRUE(d.mark_connected);
}

TEST(HandshakeDecision, StandardReply_NotAwaiting7A_IsIgnored) {
    for (HandshakePhase phase : kAllPhases) {
        if (phase == HandshakePhase::AWAIT_STANDARD) continue;
        for (bool installer_mode : {false, true}) {
            for (bool declined : {false, true}) {
                ExpectNoAction(HandshakeProtocol::standard_reply(phase, installer_mode, declined));
            }
        }
    }
}

TEST(HandshakeDecision, InstallerReply_Awaiting7B_Connects) {
    auto d = HandshakeProtocol::installer_reply(HandshakePhase::AWAIT_INSTALLER);
    EXPECT_TRUE(d.session_active);
    EXPECT_TRUE(d.cancel_installer_timeout);
    EXPECT_TRUE(d.mark_connected);
    EXPECT_FALSE(d.decline_installer);
}

TEST(HandshakeDecision, InstallerReply_NotAwaiting7B_IsIgnored) {
    for (HandshakePhase phase : kAllPhases) {
        if (phase == HandshakePhase::AWAIT_INSTALLER) continue;
        ExpectNoAction(HandshakeProtocol::installer_reply(phase));
    }
}

TEST(HandshakeDecision, InstallerTimeout_Awaiting7B_FallsBack) {
    auto d = HandshakeProtocol::installer_timeout(HandshakePhase::AWAIT_INSTALLER);
    EXPECT_TRUE(d.decline_installer);
    EXPECT_TRUE(d.mark_connected);
    EXPECT_FALSE(d.session_active);
}

TEST(HandshakeDecision, InstallerTimeout_NotAwaiting7B_IsIgnored) {
    for (HandshakePhase phase : kAllPhases) {
        if (phase == HandshakePhase::AWAIT_INSTALLER) continue;
        ExpectNoAction(HandshakeProtocol::installer_timeout(phase));
    }
}

TEST(HandshakeDecision, FirstConnectTimeout_Awaiting7A_Reconnects) {
    auto d = HandshakeProtocol::first_connect_timeout(HandshakePhase::AWAIT_STANDARD);
    EXPECT_TRUE(d.reconnect);
    EXPECT_FALSE(d.mark_connected);
}

TEST(HandshakeDecision, FirstConnectTimeout_NotAwaiting7A_IsIgnored) {
    // Once 0x7A arrived (or the link is down) a firing timer must not tear the session down.
    for (HandshakePhase phase : kAllPhases) {
        if (phase == HandshakePhase::AWAIT_STANDARD) continue;
        ExpectNoAction(HandshakeProtocol::first_connect_timeout(phase));
    }
}

TEST(HandshakeDecision, TimeoutConstants) {
    EXPECT_EQ(HandshakeProtocol::kFirstConnectTimeoutMs, 10000u);
    EXPECT_EQ(HandshakeProtocol::kInstallerConnectTimeoutMs, 5000u);
}
