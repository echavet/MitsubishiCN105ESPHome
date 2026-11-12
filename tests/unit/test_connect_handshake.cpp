/// test_connect_handshake.cpp — Tests for the CN105 connection sequence
/// (standard 0x5A, optional installer upgrade 0x5B).
/// Deps: cn105_protocol.h, frame_parser.h, cn105_types.h (production code)
///
/// Pattern: the handshake DECISIONS are the production functions
/// cn105_protocol::handshake_on_* (cn105_protocol.h), which processCommand() and
/// the installer timeout call directly. Only the glue is simulated here: a fake
/// clock, ESPHome-style named timeouts (set_timeout replaces a timer with the same
/// name, cancel_timeout removes it) and the connected/uptime bookkeeping of
/// setHeatpumpConnected(). The glue mirrors:
///   - hp_writings.cpp: sendFirstConnectionPacket(), sendInstallerConnectPacket()
///   - hp_readings.cpp: processCommand() cases 0x7a / 0x7b (applying the decision)
///   - cn105.cpp:       setHeatpumpConnected(), reconnectUART()
/// The wire frames (INST_CONNECT / CONNECT) are tested against the real
/// FrameParser and checksum.
#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "cn105_protocol.h"
#include "frame_parser.h"
#include "cn105_types.h"

namespace {

enum class State { CONNECTING, CONNECTED, DISCONNECTED };

using cn105_protocol::kFirstConnectTimeoutMs;      // "checkFirstConnection"
using cn105_protocol::kInstallerConnectTimeoutMs;  // "checkInstallerConnection"

class HandshakeSim {
public:
    // Timer callbacks capture `this`, so the object must never move or be copied.
    explicit HandshakeSim(bool installer_mode) : installer_mode_(installer_mode) {}
    HandshakeSim(const HandshakeSim&) = delete;
    HandshakeSim& operator=(const HandshakeSim&) = delete;
    HandshakeSim(HandshakeSim&&) = delete;
    HandshakeSim& operator=(HandshakeSim&&) = delete;

    // ── observable state ──
    State state = State::CONNECTING;  // setupUART() leaves the FSM in CONNECTING
    bool installer_connect_attempted = false;
    bool installer_session_active = false;
    int uptime_starts = 0;            // hp_uptime_connection_sensor_->start() calls
    int reconnects = 0;               // reconnectUART() calls
    std::vector<uint8_t> sent;        // command byte (offset 1) of each frame sent

    bool connected() const { return state == State::CONNECTED; }

    // Mirror of CN105Climate::sendFirstConnectionPacket()
    void sendFirstConnectionPacket() {
        setHeatpumpConnected(false);
        cancel_timeout("checkInstallerConnection");
        installer_connect_attempted = false;
        installer_session_active = false;
        sent.push_back(CONNECT[1]);  // 0x5A
        set_timeout("checkFirstConnection", kFirstConnectTimeoutMs, [this]() {
            // Mirror of the timeout lambda: reconnectUART() ends by re-sending CONNECT.
            ++reconnects;
            sendFirstConnectionPacket();
        });
    }

    // Mirror of CN105Climate::sendInstallerConnectPacket()
    void sendInstallerConnectPacket() {
        sent.push_back(INST_CONNECT[1]);  // 0x5B
        installer_connect_attempted = true;
        set_timeout("checkInstallerConnection", kInstallerConnectTimeoutMs, [this]() {
            if (cn105_protocol::handshake_on_installer_timeout(connected()).mark_connected) {
                setHeatpumpConnected(true);
            }
        });
    }

    // Mirror of the glue in CN105Climate::processCommand(): ask the production
    // decision function, then apply the result.
    void onFrame(uint8_t command) {
        switch (command) {
        case 0x7a: {
            const auto d = cn105_protocol::handshake_on_standard_reply(installer_mode_);
            if (d.cancel_first_timeout) cancel_timeout("checkFirstConnection");
            if (d.send_installer) sendInstallerConnectPacket();
            if (d.mark_connected) setHeatpumpConnected(true);
            break;
        }
        case 0x7b: {
            const auto d = cn105_protocol::handshake_on_installer_reply(
                installer_connect_attempted, connected());
            if (d.session_active) installer_session_active = true;
            if (d.cancel_installer_timeout) cancel_timeout("checkInstallerConnection");
            if (d.mark_connected) setHeatpumpConnected(true);
            break;
        }
        default:
            break;
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

    // Mirror of CN105Climate::setHeatpumpConnected()
    void setHeatpumpConnected(bool connected_now) {
        if (connected_now) {
            state = State::CONNECTED;
            ++uptime_starts;
        } else {
            if (state == State::CONNECTED) state = State::DISCONNECTED;
        }
    }

    bool installer_mode_;
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

TEST(ConnectHandshakeFrames, InstallerConnectChecksumRecomputes) {
    // sendInstallerConnectPacket() recomputes the checksum at runtime; it must
    // agree with the constant.
    EXPECT_EQ(cn105_protocol::checksum(INST_CONNECT, INST_CONNECT_LEN - 1),
              INST_CONNECT[INST_CONNECT_LEN - 1]);
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
    EXPECT_FALSE(sim.installer_connect_attempted);
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

    EXPECT_FALSE(sim.installer_session_active);
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
    EXPECT_TRUE(sim.installer_connect_attempted);
    EXPECT_FALSE(sim.connected());
    EXPECT_FALSE(sim.installer_session_active);
}

TEST(ConnectHandshake, Installer_7BCompletesSession) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);

    sim.onFrame(0x7b);

    EXPECT_TRUE(sim.connected());
    EXPECT_TRUE(sim.installer_session_active);
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
    EXPECT_FALSE(sim.installer_session_active);
    EXPECT_EQ(sim.uptime_starts, 1);
    EXPECT_EQ(sim.reconnects, 0);  // fallback must not tear the link down
}

TEST(ConnectHandshake, Installer_Late7BAfterFallback_DoesNotRestartUptime) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.advance(kInstallerConnectTimeoutMs);
    ASSERT_TRUE(sim.connected());
    ASSERT_EQ(sim.uptime_starts, 1);

    sim.onFrame(0x7b);  // arrives after we already fell back

    EXPECT_TRUE(sim.connected());
    EXPECT_TRUE(sim.installer_session_active);  // session did upgrade after all
    EXPECT_EQ(sim.uptime_starts, 1);            // uptime counter not reset
}

TEST(ConnectHandshake, Installer_Unsolicited7BBefore7A_IsIgnored) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver

    sim.onFrame(0x7b);

    EXPECT_FALSE(sim.connected());
    EXPECT_FALSE(sim.installer_session_active);
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

TEST(ConnectHandshake, ReconnectResetsInstallerFlags) {
    HandshakeSim sim(true);
    sim.sendFirstConnectionPacket();  // CONNECT goes out immediately, as in the driver
    sim.onFrame(0x7a);
    sim.onFrame(0x7b);
    ASSERT_TRUE(sim.installer_session_active);

    sim.sendFirstConnectionPacket();

    EXPECT_FALSE(sim.installer_connect_attempted);
    EXPECT_FALSE(sim.installer_session_active);
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
    EXPECT_TRUE(sim.installer_session_active);
    EXPECT_EQ(sim.sent, (std::vector<uint8_t>{0x5A, 0x5B, 0x5A, 0x5B}));
    EXPECT_EQ(sim.uptime_starts, 2);
}

// ════════════════════════════════════════════════════════════════
// Decision functions — direct truth tables (production code)
// ════════════════════════════════════════════════════════════════

TEST(HandshakeDecision, StandardReply_StandardMode_ConnectsImmediately) {
    auto d = cn105_protocol::handshake_on_standard_reply(/*installer_mode=*/false);
    EXPECT_TRUE(d.cancel_first_timeout);
    EXPECT_TRUE(d.mark_connected);
    EXPECT_FALSE(d.send_installer);
    EXPECT_FALSE(d.session_active);
    EXPECT_FALSE(d.cancel_installer_timeout);
}

TEST(HandshakeDecision, StandardReply_InstallerMode_UpgradesInsteadOfConnecting) {
    auto d = cn105_protocol::handshake_on_standard_reply(/*installer_mode=*/true);
    EXPECT_TRUE(d.cancel_first_timeout);
    EXPECT_TRUE(d.send_installer);
    EXPECT_FALSE(d.mark_connected);
    EXPECT_FALSE(d.session_active);
}

TEST(HandshakeDecision, InstallerReply_NotAttempted_IsIgnored) {
    for (bool connected : {false, true}) {
        auto d = cn105_protocol::handshake_on_installer_reply(false, connected);
        EXPECT_FALSE(d.session_active);
        EXPECT_FALSE(d.cancel_installer_timeout);
        EXPECT_FALSE(d.mark_connected);
        EXPECT_FALSE(d.send_installer);
    }
}

TEST(HandshakeDecision, InstallerReply_Attempted_NotYetConnected_Connects) {
    auto d = cn105_protocol::handshake_on_installer_reply(true, false);
    EXPECT_TRUE(d.session_active);
    EXPECT_TRUE(d.cancel_installer_timeout);
    EXPECT_TRUE(d.mark_connected);
}

TEST(HandshakeDecision, InstallerReply_Attempted_AlreadyConnected_DoesNotReconnect) {
    // Late 0x7B after the timeout fallback: upgrade the session, keep uptime.
    auto d = cn105_protocol::handshake_on_installer_reply(true, true);
    EXPECT_TRUE(d.session_active);
    EXPECT_TRUE(d.cancel_installer_timeout);
    EXPECT_FALSE(d.mark_connected);
}

TEST(HandshakeDecision, InstallerTimeout_ConnectsOnlyIfNotAlready) {
    EXPECT_TRUE(cn105_protocol::handshake_on_installer_timeout(false).mark_connected);
    EXPECT_FALSE(cn105_protocol::handshake_on_installer_timeout(true).mark_connected);
    EXPECT_FALSE(cn105_protocol::handshake_on_installer_timeout(false).session_active);
}

TEST(HandshakeDecision, TimeoutConstants) {
    EXPECT_EQ(cn105_protocol::kFirstConnectTimeoutMs, 10000u);
    EXPECT_EQ(cn105_protocol::kInstallerConnectTimeoutMs, 5000u);
}
