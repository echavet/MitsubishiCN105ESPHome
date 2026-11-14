#pragma once
/// connect_handshake.h — CN105 connection handshake (standard 0x5A, optional installer 0x5B).
///
/// HandshakeProtocol's pure decision functions say what to do for each event;
/// ConnectHandshake owns the per-connection state and both timers and applies those
/// decisions. Nothing here depends on ESPHome: the driver supplies packets, timers,
/// the connected flag and logging through HandshakeHooks, so unit tests can drive
/// this exact code with a fake clock.

#include <cstdint>
#include <functional>
#include <utility>

namespace esphome::cn105 {

// ════════════════════════════════════════════════════════════════
// Connection handshake decisions (pure, testable)
// ════════════════════════════════════════════════════════════════
//
// Sequence: CONNECT 0x5A -> reply 0x7A. When installer_mode is on, the driver then
// sends 0x5B and waits for 0x7B; if no 0x7B arrives in time, a timeout falls back to
// the standard session and marks installer mode as declined, so later reconnects skip
// 0x5B (ConnectHandshake keeps that flag for the life of the boot).
// The unit only answers commands, so a reply is valid only while its request is
// pending: 0x7A after 0x5A, 0x7B after 0x5B. Any other 0x7A/0x7B is stray and is
// ignored without changing the handshake or connection state.
// The poll cycle starts when mark_connected is requested.
// These functions only DECIDE; ConnectHandshake (below) applies them.

/// Which CONNECT request, if any, is waiting for its reply.
enum class HandshakePhase : uint8_t {
    IDLE,             // nothing sent yet, or the link was torn down
    AWAIT_STANDARD,   // 0x5A sent, waiting for 0x7A
    AWAIT_INSTALLER,  // 0x5B sent, waiting for 0x7B
    DONE,             // session established: no handshake reply is expected
};

struct HandshakeProtocol {
    /// Time to wait for the 0x7A reply before reconnecting ("checkFirstConnection").
    static constexpr uint32_t kFirstConnectTimeoutMs = 10000;
    /// Time to wait for the 0x7B reply before falling back ("checkInstallerConnection").
    static constexpr uint32_t kInstallerConnectTimeoutMs = 5000;

    struct Decision {
        bool begin_session = false;             // cancel "checkFirstConnection" and reset cached state
        bool send_installer = false;            // send INST_CONNECT (0x5B) and arm its timeout
        bool cancel_installer_timeout = false;  // cancel "checkInstallerConnection"
        bool session_active = false;            // installer session is established
        bool decline_installer = false;         // unit did not answer 0x5B: skip it on later reconnects
        bool mark_connected = false;            // call setHeatpumpConnected(true)
        bool reconnect = false;                 // no 0x7A in time: tear down and resend 0x5A
    };

    /// The 0x7A reply timeout expired.
    /// @param phase  Request currently waiting for a reply.
    static Decision first_connect_timeout(HandshakePhase phase) {
        Decision decision;
        decision.reconnect = phase == HandshakePhase::AWAIT_STANDARD;  // otherwise stale timer: ignore
        return decision;
    }

    /// A 0x7A (standard connect success) frame arrived.
    /// @param phase                Request currently waiting for a reply.
    /// @param installer_mode       YAML `installer_mode` is enabled.
    /// @param installer_declined   The unit did not answer 0x5B earlier in this boot.
    static Decision standard_reply(HandshakePhase phase, bool installer_mode, bool installer_declined) {
        Decision decision;
        if (phase != HandshakePhase::AWAIT_STANDARD) return decision;  // stray: ignore
        const bool upgrade = installer_mode && !installer_declined;
        decision.begin_session = true;
        decision.send_installer = upgrade;
        decision.mark_connected = !upgrade;  // the upgrade defers until 0x7B / timeout
        return decision;
    }

    /// A 0x7B (installer connect success) frame arrived.
    /// @param phase  Request currently waiting for a reply.
    static Decision installer_reply(HandshakePhase phase) {
        Decision decision;
        if (phase != HandshakePhase::AWAIT_INSTALLER) return decision;  // stray: ignore
        decision.session_active = true;
        decision.cancel_installer_timeout = true;
        decision.mark_connected = true;
        return decision;
    }

    /// The 0x5B reply timeout expired: keep the standard session and start polling.
    /// @param phase  Request currently waiting for a reply.
    static Decision installer_timeout(HandshakePhase phase) {
        Decision decision;
        if (phase != HandshakePhase::AWAIT_INSTALLER) return decision;  // stale timer: ignore
        decision.decline_installer = true;
        decision.mark_connected = true;
        return decision;
    }
};

// ════════════════════════════════════════════════════════════════
// Handshake driver: state, timers and decision application
// ════════════════════════════════════════════════════════════════

/// Outcomes the driver reacts to (logging, cache reset). Emitted before the
/// decision is applied, so SESSION_STARTED runs before the link is marked connected.
enum class HandshakeEvent : uint8_t {
    SESSION_STARTED,       // 0x7A answered 0x5A: reset cached state
    INSTALLER_SKIPPED,     // installer_mode on, but the unit declined 0x5B earlier this boot
    INSTALLER_TIMEOUT,     // no 0x7B reply to 0x5B
    INSTALLER_ACTIVE,      // 0x7B answered 0x5B: installer session established
    UNEXPECTED_STANDARD,   // stray 0x7A ignored (no 0x5A pending)
    UNEXPECTED_INSTALLER,  // stray 0x7B ignored (no 0x5B pending)
};

struct HandshakeHooks {
    std::function<void()> send_installer_connect;  // write INST_CONNECT (0x5B)
    std::function<void(const char*, uint32_t, std::function<void()>)> set_timeout;
    std::function<void(const char*)> cancel_timeout;
    std::function<void()> mark_connected;            // setHeatpumpConnected(true)
    std::function<void()> on_first_connect_timeout;  // no 0x7A in time: reconnect
    std::function<void(HandshakeEvent)> on_event;
};

class ConnectHandshake {
public:
    static constexpr const char* kFirstConnectTimer = "checkFirstConnection";
    static constexpr const char* kInstallerTimer = "checkInstallerConnection";

    explicit ConnectHandshake(HandshakeHooks hooks) : hooks_(std::move(hooks)) {}
    // Timer callbacks capture `this`, so the object must never move or be copied.
    ConnectHandshake(const ConnectHandshake&) = delete;
    ConnectHandshake& operator=(const ConnectHandshake&) = delete;

    void set_installer_mode(bool on) { installer_mode_ = on; }
    HandshakePhase phase() const { return phase_; }
    bool installer_session_active() const { return installer_session_active_; }
    bool installer_declined() const { return installer_declined_; }

    /// CONNECT (0x5A) was just written: start a new attempt and wait for 0x7A.
    void on_connect_sent() {
        reset_connection_();
        phase_ = HandshakePhase::AWAIT_STANDARD;
        hooks_.set_timeout(kFirstConnectTimer, HandshakeProtocol::kFirstConnectTimeoutMs, [this]() {
            if (HandshakeProtocol::first_connect_timeout(phase_).reconnect) hooks_.on_first_connect_timeout();
        });
    }

    /// The link was torn down. installer_declined survives: it lasts until reboot.
    void on_disconnect() {
        reset_connection_();
        hooks_.cancel_timeout(kFirstConnectTimer);
    }

    /// A 0x7A frame accepted by the protocol profile arrived.
    void on_standard_reply() {
        const auto decision = HandshakeProtocol::standard_reply(phase_, installer_mode_, installer_declined_);
        if (!decision.begin_session) {
            emit_(HandshakeEvent::UNEXPECTED_STANDARD);
            return;
        }
        emit_(HandshakeEvent::SESSION_STARTED);
        if (installer_mode_ && !decision.send_installer) emit_(HandshakeEvent::INSTALLER_SKIPPED);
        apply_(decision);
    }

    /// A 0x7B frame accepted by the protocol profile arrived.
    void on_installer_reply() {
        const auto decision = HandshakeProtocol::installer_reply(phase_);
        if (!decision.session_active) {
            emit_(HandshakeEvent::UNEXPECTED_INSTALLER);
            return;
        }
        emit_(HandshakeEvent::INSTALLER_ACTIVE);
        apply_(decision);
    }

private:
    void reset_connection_() {
        hooks_.cancel_timeout(kInstallerTimer);
        phase_ = HandshakePhase::IDLE;
        installer_session_active_ = false;
    }

    void send_installer_() {
        phase_ = HandshakePhase::AWAIT_INSTALLER;
        hooks_.send_installer_connect();
        hooks_.set_timeout(kInstallerTimer, HandshakeProtocol::kInstallerConnectTimeoutMs, [this]() {
            const auto decision = HandshakeProtocol::installer_timeout(phase_);
            if (!decision.mark_connected) return;
            emit_(HandshakeEvent::INSTALLER_TIMEOUT);
            apply_(decision);
        });
    }

    void apply_(const HandshakeProtocol::Decision& decision) {
        if (decision.begin_session) hooks_.cancel_timeout(kFirstConnectTimer);
        if (decision.cancel_installer_timeout) hooks_.cancel_timeout(kInstallerTimer);
        if (decision.session_active) installer_session_active_ = true;
        if (decision.decline_installer) installer_declined_ = true;
        if (decision.send_installer) send_installer_();
        if (decision.mark_connected) {
            phase_ = HandshakePhase::DONE;
            hooks_.mark_connected();
        }
    }

    void emit_(HandshakeEvent event) {
        if (hooks_.on_event) hooks_.on_event(event);
    }

    HandshakeHooks hooks_;
    bool installer_mode_ = false;
    HandshakePhase phase_ = HandshakePhase::IDLE;
    bool installer_session_active_ = false;
    bool installer_declined_ = false;
};

}  // namespace esphome::cn105
