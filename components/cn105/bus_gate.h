#pragma once
/// bus_gate.h — Single-outstanding-request gate for the heat pump UART.
///
/// The heat pump answers one request at a time and its 0x61 acknowledgements carry no
/// identity, so replies can only be matched to requests by order. BusGate records the one
/// request that is waiting for a reply. Every sender must call begin() first and may only
/// write when it returns true. Replies are matched with complete().
///
/// A reply that does not match the outstanding request is stray and must be ignored.
/// The reply timeout is long enough that a request still outstanding after it is assumed
/// lost: late replies are not handled. Measured heat pump replies take 214 to 224 ms against
/// a 2000 ms timeout.
///
/// Nothing here depends on ESPHome. Time is passed in, and unsigned subtraction keeps
/// every age correct across millis() rollover.

#include <cstdint>
#include <optional>

namespace esphome::cn105 {

enum class BusRequestKind : uint8_t {
    NONE,
    INFO_REQUEST,
    SET_SETTINGS,
    SET_RUN_STATES,
    SET_REMOTE_TEMPERATURE,
    SET_FUNCTIONS_PART1,
    SET_FUNCTIONS_PART2,
};

enum class BusExpectedResponse : uint8_t {
    ACK,
    DATA,
    EITHER,
};

enum class BusResponse : uint8_t {
    ACK,
    DATA,
};

class BusGate {
public:
    static constexpr uint32_t kReplyTimeoutMs = 2000;
    /// "Any reply of the expected type" (ACKs carry no identity).
    static constexpr uint16_t kAnyCode = UINT16_MAX;

    /// Claim the bus for one request.
    /// @param timeout_ms  How long to wait for the reply before treating the request as lost.
    /// @return false when another request is still waiting for its reply.
    bool begin(BusRequestKind kind, BusExpectedResponse expected_response, uint32_t now_ms,
               uint32_t timeout_ms = kReplyTimeoutMs, uint16_t expected_code = kAnyCode) {
        if (this->is_outstanding_(now_ms)) {
            return false;
        }
        this->outstanding_kind_ = kind;
        this->expected_response_ = expected_response;
        this->expected_code_ = expected_code;
        this->started_ms_ = now_ms;
        this->timeout_ms_ = timeout_ms;
        return true;
    }

    /// True when no request is waiting. A request past the timeout counts as gone.
    bool is_idle(uint32_t now_ms) const {
        return !this->is_outstanding_(now_ms);
    }

    /// Kind of the request currently recorded, whether or not it has timed out.
    BusRequestKind outstanding_kind() const {
        return this->outstanding_kind_;
    }

    /// Match a reply against the outstanding request and clear it on success.
    /// @return the kind that was completed, or std::nullopt when the reply is stray
    ///         (nothing outstanding, timed out, or the wrong type of reply).
    /// @param code  Code carried by the reply (data[0] of a 0x62 frame), or kAnyCode for ACKs.
    std::optional<BusRequestKind> complete(BusResponse response, uint32_t now_ms, uint16_t code = kAnyCode) {
        if (!this->is_outstanding_(now_ms)) {
            return std::nullopt;
        }
        if (!this->accepts_(response)) {
            return std::nullopt;
        }
        // A reply that names a different request than the one we wait for is stray.
        if (this->expected_code_ != kAnyCode && code != this->expected_code_) {
            return std::nullopt;
        }
        const BusRequestKind completed_kind = this->outstanding_kind_;
        this->clear_();
        return completed_kind;
    }

    /// Collect a request that timed out. Call every loop pass so the owner can react.
    /// @return the kind that timed out, or std::nullopt when nothing did.
    std::optional<BusRequestKind> expire(uint32_t now_ms) {
        if (this->outstanding_kind_ == BusRequestKind::NONE) {
            return std::nullopt;
        }
        if (this->is_outstanding_(now_ms)) {
            return std::nullopt;
        }
        const BusRequestKind expired_kind = this->outstanding_kind_;
        this->clear_();
        return expired_kind;
    }

    /// Forget the outstanding request: soft timeout, disconnect or reconnect.
    void release() {
        this->clear_();
    }

private:
    bool is_outstanding_(uint32_t now_ms) const {
        if (this->outstanding_kind_ == BusRequestKind::NONE) {
            return false;
        }
        const uint32_t age_ms = now_ms - this->started_ms_;
        return age_ms < this->timeout_ms_;
    }

    bool accepts_(BusResponse response) const {
        switch (this->expected_response_) {
        case BusExpectedResponse::ACK:
            return response == BusResponse::ACK;
        case BusExpectedResponse::DATA:
            return response == BusResponse::DATA;
        case BusExpectedResponse::EITHER:
            return true;
        }
        return false;
    }

    void clear_() {
        this->outstanding_kind_ = BusRequestKind::NONE;
    }

    BusRequestKind outstanding_kind_ = BusRequestKind::NONE;
    BusExpectedResponse expected_response_ = BusExpectedResponse::EITHER;
    uint16_t expected_code_ = kAnyCode;
    uint32_t started_ms_ = 0;
    uint32_t timeout_ms_ = kReplyTimeoutMs;
};

}  // namespace esphome::cn105
