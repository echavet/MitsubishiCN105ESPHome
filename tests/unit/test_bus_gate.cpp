/// test_bus_gate.cpp — Tests for esphome::cn105::BusGate (production code, no mocks needed).
///
/// BusGate enforces "one request waiting for a reply at a time". Replies are matched to
/// requests by order only, so the tests check that a reply is never consumed by the wrong
/// request, and that a request past the reply timeout is treated as lost.
#include <gtest/gtest.h>
#include <cstdint>
#include "bus_gate.h"

using esphome::cn105::BusExpectedResponse;
using esphome::cn105::BusGate;
using esphome::cn105::BusRequestKind;
using esphome::cn105::BusResponse;

namespace {
constexpr uint32_t kTimeoutMs = BusGate::kReplyTimeoutMs;
constexpr uint32_t kStartMs = 10000;
}

// ════════════════════════════════════════════════════════════════
// Basic claim and release
// ════════════════════════════════════════════════════════════════

TEST(BusGate, StartsIdle) {
    BusGate gate;
    EXPECT_TRUE(gate.is_idle(kStartMs));
    EXPECT_EQ(gate.outstanding_kind(), BusRequestKind::NONE);
}

TEST(BusGate, BeginClaimsTheBus) {
    BusGate gate;
    EXPECT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    EXPECT_FALSE(gate.is_idle(kStartMs));
    EXPECT_EQ(gate.outstanding_kind(), BusRequestKind::SET_SETTINGS);
}

TEST(BusGate, SecondBeginIsRefusedWhileOutstanding) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    EXPECT_FALSE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA, kStartMs + 1));
    EXPECT_EQ(gate.outstanding_kind(), BusRequestKind::SET_SETTINGS);
}

TEST(BusGate, CompleteFreesTheBus) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    const auto completed = gate.complete(BusResponse::ACK, kStartMs + 100);
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, BusRequestKind::SET_SETTINGS);
    EXPECT_TRUE(gate.is_idle(kStartMs + 100));
    EXPECT_TRUE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA, kStartMs + 100));
}

TEST(BusGate, ReleaseFreesTheBus) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA, kStartMs));
    gate.release();
    EXPECT_TRUE(gate.is_idle(kStartMs));
    EXPECT_EQ(gate.outstanding_kind(), BusRequestKind::NONE);
}

// ════════════════════════════════════════════════════════════════
// Reply matching
// ════════════════════════════════════════════════════════════════

TEST(BusGate, ReplyWithNothingOutstandingIsStray) {
    BusGate gate;
    EXPECT_FALSE(gate.complete(BusResponse::ACK, kStartMs).has_value());
    EXPECT_FALSE(gate.complete(BusResponse::DATA, kStartMs).has_value());
}

TEST(BusGate, AckDoesNotCompleteADataRequest) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA, kStartMs));
    EXPECT_FALSE(gate.complete(BusResponse::ACK, kStartMs + 10).has_value());
    EXPECT_FALSE(gate.is_idle(kStartMs + 10));
    EXPECT_TRUE(gate.complete(BusResponse::DATA, kStartMs + 20).has_value());
}

TEST(BusGate, DataDoesNotCompleteAnAckRequest) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    EXPECT_FALSE(gate.complete(BusResponse::DATA, kStartMs + 10).has_value());
    EXPECT_FALSE(gate.is_idle(kStartMs + 10));
    EXPECT_TRUE(gate.complete(BusResponse::ACK, kStartMs + 20).has_value());
}

TEST(BusGate, DuplicateReplyIsStray) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    ASSERT_TRUE(gate.complete(BusResponse::ACK, kStartMs + 10).has_value());
    EXPECT_FALSE(gate.complete(BusResponse::ACK, kStartMs + 20).has_value());
}

TEST(BusGateCode, ReplyForAnotherCodeIsStrayAndKeepsTheClaim) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA,
                           kStartMs, kTimeoutMs, 0x06));           // B = status
    EXPECT_FALSE(gate.complete(BusResponse::DATA, kStartMs + 10, 0x09).has_value()); // late A
    EXPECT_FALSE(gate.is_idle(kStartMs + 10));                      // still waiting for B
    EXPECT_TRUE(gate.complete(BusResponse::DATA, kStartMs + 20, 0x06).has_value());
}

TEST(BusGateCode, AckRequestsIgnoreTheCode) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    EXPECT_TRUE(gate.complete(BusResponse::ACK, kStartMs + 10).has_value());
}

// ════════════════════════════════════════════════════════════════
// Timeout: a request past the timeout is assumed lost
// ════════════════════════════════════════════════════════════════

TEST(BusGate, StillOutstandingJustBeforeTimeout) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    EXPECT_FALSE(gate.is_idle(kStartMs + kTimeoutMs - 1));
    EXPECT_FALSE(gate.expire(kStartMs + kTimeoutMs - 1).has_value());
}

TEST(BusGate, IdleExactlyAtTimeout) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    EXPECT_TRUE(gate.is_idle(kStartMs + kTimeoutMs));
}

TEST(BusGate, ExpireReportsTheKindOnceAndClears) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_FUNCTIONS_PART2, BusExpectedResponse::ACK, kStartMs));
    const auto expired = gate.expire(kStartMs + kTimeoutMs);
    ASSERT_TRUE(expired.has_value());
    EXPECT_EQ(*expired, BusRequestKind::SET_FUNCTIONS_PART2);
    EXPECT_FALSE(gate.expire(kStartMs + kTimeoutMs + 1).has_value());
    EXPECT_EQ(gate.outstanding_kind(), BusRequestKind::NONE);
}

TEST(BusGate, ExpireWithNothingOutstandingReportsNothing) {
    BusGate gate;
    EXPECT_FALSE(gate.expire(kStartMs).has_value());
}

TEST(BusGate, ReplyAfterTimeoutIsIgnored) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    EXPECT_FALSE(gate.complete(BusResponse::ACK, kStartMs + kTimeoutMs).has_value());
}

TEST(BusGate, ReplyAfterTimeoutAndExpireIsIgnored) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    ASSERT_TRUE(gate.expire(kStartMs + kTimeoutMs).has_value());
    EXPECT_FALSE(gate.complete(BusResponse::ACK, kStartMs + kTimeoutMs + 5).has_value());
}

TEST(BusGate, BeginReplacesARequestThatTimedOut) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    EXPECT_TRUE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA, kStartMs + kTimeoutMs));
    EXPECT_EQ(gate.outstanding_kind(), BusRequestKind::INFO_REQUEST);
}

TEST(BusGate, TimeoutSurvivesMillisRollover) {
    const uint32_t startBeforeWrapMs = UINT32_MAX - 500;
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, startBeforeWrapMs));
    const uint32_t justBeforeTimeoutMs = startBeforeWrapMs + kTimeoutMs - 1;
    const uint32_t atTimeoutMs = startBeforeWrapMs + kTimeoutMs;
    EXPECT_FALSE(gate.is_idle(justBeforeTimeoutMs));
    EXPECT_TRUE(gate.is_idle(atTimeoutMs));
    EXPECT_TRUE(gate.complete(BusResponse::ACK, justBeforeTimeoutMs).has_value());
}

// ════════════════════════════════════════════════════════════════
// The holes the gate closes
// ════════════════════════════════════════════════════════════════

TEST(BusGateScenario, FunctionsWriteCannotStartWhileSettingsAckPending) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));

    EXPECT_FALSE(gate.begin(BusRequestKind::SET_FUNCTIONS_PART1, BusExpectedResponse::ACK, kStartMs + 50));

    const auto completed = gate.complete(BusResponse::ACK, kStartMs + 150);
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(*completed, BusRequestKind::SET_SETTINGS);

    EXPECT_TRUE(gate.begin(BusRequestKind::SET_FUNCTIONS_PART1, BusExpectedResponse::ACK, kStartMs + 160));
}

TEST(BusGateScenario, RemoteTemperatureAckIsNotTakenForAFunctionsAck) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_REMOTE_TEMPERATURE, BusExpectedResponse::ACK, kStartMs));
    EXPECT_FALSE(gate.begin(BusRequestKind::SET_FUNCTIONS_PART1, BusExpectedResponse::ACK, kStartMs + 10));

    const auto remoteAck = gate.complete(BusResponse::ACK, kStartMs + 120);
    ASSERT_TRUE(remoteAck.has_value());
    EXPECT_EQ(*remoteAck, BusRequestKind::SET_REMOTE_TEMPERATURE);

    ASSERT_TRUE(gate.begin(BusRequestKind::SET_FUNCTIONS_PART1, BusExpectedResponse::ACK, kStartMs + 130));
    const auto part1Ack = gate.complete(BusResponse::ACK, kStartMs + 230);
    ASSERT_TRUE(part1Ack.has_value());
    EXPECT_EQ(*part1Ack, BusRequestKind::SET_FUNCTIONS_PART1);
}

TEST(BusGateScenario, FunctionsPartsAreSentOneAtATime) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_FUNCTIONS_PART1, BusExpectedResponse::ACK, kStartMs));
    EXPECT_FALSE(gate.begin(BusRequestKind::SET_FUNCTIONS_PART2, BusExpectedResponse::ACK, kStartMs + 10));

    ASSERT_TRUE(gate.complete(BusResponse::ACK, kStartMs + 100).has_value());
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_FUNCTIONS_PART2, BusExpectedResponse::ACK, kStartMs + 110));

    const auto part2Ack = gate.complete(BusResponse::ACK, kStartMs + 200);
    ASSERT_TRUE(part2Ack.has_value());
    EXPECT_EQ(*part2Ack, BusRequestKind::SET_FUNCTIONS_PART2);
}

TEST(BusGateScenario, LostFunctionsAckTimesOutAndFreesTheBus) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_FUNCTIONS_PART1, BusExpectedResponse::ACK, kStartMs));

    const auto expired = gate.expire(kStartMs + kTimeoutMs);
    ASSERT_TRUE(expired.has_value());
    EXPECT_EQ(*expired, BusRequestKind::SET_FUNCTIONS_PART1);
    EXPECT_TRUE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA, kStartMs + kTimeoutMs));
}

TEST(BusGateScenario, PollCannotStartWhileASetIsPending) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_REMOTE_TEMPERATURE, BusExpectedResponse::ACK, kStartMs));
    EXPECT_FALSE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA, kStartMs + 500));
}

TEST(BusGateScenario, DisconnectDropsTheOutstandingRequest) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA, kStartMs));
    gate.release();
    EXPECT_FALSE(gate.complete(BusResponse::DATA, kStartMs + 10).has_value());
    EXPECT_TRUE(gate.begin(BusRequestKind::INFO_REQUEST, BusExpectedResponse::DATA, kStartMs + 20));
}

// ════════════════════════════════════════════════════════════════
// Per-request timeout
// ════════════════════════════════════════════════════════════════

TEST(BusGateTimeout, DefaultTimeoutIsUnchanged) {
    BusGate gate;
    ASSERT_TRUE(gate.begin(BusRequestKind::SET_SETTINGS, BusExpectedResponse::ACK, kStartMs));
    EXPECT_FALSE(gate.is_idle(kStartMs + kTimeoutMs - 1));
    EXPECT_TRUE(gate.is_idle(kStartMs + kTimeoutMs));
}
