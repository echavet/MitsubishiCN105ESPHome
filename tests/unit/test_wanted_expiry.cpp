#include <gtest/gtest.h>
#include "cn105_protocol.h"

using cn105_protocol::is_wanted_settings_expired;

TEST(WantedExpiry, FreshIsNotExpired)        { EXPECT_FALSE(is_wanted_settings_expired(1000, 2000, 30000)); }
TEST(WantedExpiry, ExactlyAtLimitIsKept)     { EXPECT_FALSE(is_wanted_settings_expired(1000, 31000, 30000)); }
TEST(WantedExpiry, PastLimitIsExpired)       { EXPECT_TRUE(is_wanted_settings_expired(1000, 31001, 30000)); }
TEST(WantedExpiry, SurvivesMillisRollover) {
    const uint32_t changed = UINT32_MAX - 1000;
    EXPECT_FALSE(is_wanted_settings_expired(changed, changed + 30000, 30000));
    EXPECT_TRUE(is_wanted_settings_expired(changed, changed + 30001, 30000));
}
