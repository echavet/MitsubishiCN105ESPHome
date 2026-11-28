#include "request_scheduler.h"
#include "cn105_types.h"
#include <cinttypes>
#ifndef CN105_UNIT_TEST
#include "Globals.h"
#include "cn105.h"
#include <esphome.h>
#endif

using namespace esphome;

RequestScheduler::RequestScheduler(
    SendCallback send_callback,
    TimeoutCallback timeout_callback,
    TerminateCallback terminate_callback,
    ContextCallback context_callback,
    AbandonCallback abandon_callback
) : current_request_index_(-1),
send_callback_(send_callback),
timeout_callback_(timeout_callback),
terminate_callback_(terminate_callback),
context_callback_(context_callback),
abandon_callback_(abandon_callback) {}

void RequestScheduler::register_request(InfoRequest& req) {
    requests_.push_back(req);
}

void RequestScheduler::clear_requests() {
    requests_.clear();
    current_request_index_ = -1;
}

void RequestScheduler::disable_request(uint8_t code) {
    for (auto& req : requests_) {
        if (req.code == code) {
            req.disabled = true;
            break;
        }
    }
}

void RequestScheduler::enable_request(uint8_t code) {
    for (auto& req : requests_) {
        if (req.code == code) {
            req.disabled = false;
            break;
        }
    }
}

void RequestScheduler::timer_bypass(uint8_t code) {
    for (auto& req : requests_) {
        if (req.code == code) {
            req.last_request_time = 0;
            break;
        }
    }
}

bool RequestScheduler::is_empty() const {
    return requests_.empty();
}

void RequestScheduler::send_request(uint8_t code, CN105Climate* context) {
    // Get context if not provided but callback is available
    if (!context && context_callback_) {
        context = context_callback_();
    }

    for (size_t i = 0; i < requests_.size(); ++i) {
        auto& req = requests_[i];
        if (req.code != code) continue;
        if (req.disabled) { return; }

        // Check canSend if present and if context is available
        if (req.canSend && context) {
            if (!req.canSend(*context)) {
                return;
            }
        }

        const char* tag = req.log_tag ? req.log_tag : LOG_CYCLE_TAG;
        ESP_LOGD(tag, "Sending %s (0x%02X)", req.description, req.code);

        // Send the packet via callback
        if (send_callback_ && !send_callback_(req.code)) {
            // nothing went out: leave awaiting / last_request_time untouched
            return;
        }
        req.awaiting = true;
        req.last_request_time = CUSTOM_MILLIS;

        // Manage the timeout if configured and if the callback is available
        if (req.soft_timeout_ms > 0 && timeout_callback_) {
            uint8_t code_copy = req.code;
            // set_timeout keeps the name pointer, so pass the stored string, not a temporary
            timeout_callback_(req.timeout_name, req.soft_timeout_ms, [this, code_copy]() {
                // Get context for send_next_after
                CN105Climate* ctx = nullptr;
                if (this->context_callback_) {
                    ctx = this->context_callback_();
                }

                // If the response is still expected, consider it a soft failure and continue
                for (auto& r : this->requests_) {
                    if (r.code == code_copy && r.awaiting) {
                        r.awaiting = false;
                        r.failures++;
                        ESP_LOGW(LOG_CYCLE_TAG, "Soft timeout for %s (0x%02X), failures: %d",
                            r.description, r.code, r.failures);
                        if (r.failures >= r.maxFailures) {
                            r.disabled = true;
                            ESP_LOGW(LOG_CYCLE_TAG, "%s (0x%02X) disabled (not supported)",
                                r.description, r.code);
                        }
                        if (this->abandon_callback_) {
                            this->abandon_callback_();
                        }
                        this->send_next_after(code_copy, ctx);
                        break;
                    }
                }
                });
        }

        current_request_index_ = static_cast<int>(i);
        return;
    }
}

void RequestScheduler::mark_response_seen(uint8_t code, CN105Climate* context) {
    // Get context if not provided but callback is available
    if (!context && context_callback_) {
        context = context_callback_();
    }

    for (auto& req : requests_) {
        if (req.code == code) {
            req.awaiting = false;
            req.failures = 0;
            ESP_LOGD(LOG_CYCLE_TAG, "Received %s <0x%02X>", req.description, req.code);

            // Call the onResponse callback if present and if the context is available
            if (req.onResponse && context) {
                req.onResponse(*context);
            }
            return;
        }
    }
}

void RequestScheduler::send_next_after(uint8_t previous_code, CN105Climate* context) {
    // Get context if not provided but callback is available
    if (!context && context_callback_) {
        context = context_callback_();
    }

    // Find the starting index (by code) then try the next activateable entries in order
    int start = -1;
    for (size_t i = 0; i < requests_.size(); ++i) {
        if (requests_[i].code == previous_code) {
            start = static_cast<int>(i);
            break;
        }
    }
    int idx = (start < 0) ? 0 : start + 1;

    for (; idx < static_cast<int>(requests_.size()); ++idx) {
        auto& req = requests_[idx];
        if (req.disabled) {
            if (req.log_tag) {
                ESP_LOGD(req.log_tag, "Skipping %s (0x%02X): disabled", req.description, req.code);
            }
            continue;
        }

        // Check canSend if present and if context is available
        if (req.canSend && context) {
            if (!req.canSend(*context)) {
                if (req.log_tag) {
                    ESP_LOGD(req.log_tag, "Skipping %s (0x%02X): canSend returned false", req.description, req.code);
                }
                continue;
            }
        }

        if (req.interval_ms > 0 && (CUSTOM_MILLIS - req.last_request_time < req.interval_ms) && req.last_request_time > 0) {
            if (req.log_tag) {
                ESP_LOGD(req.log_tag, "Skipping %s (0x%02X) - interval not elapsed (elapsed: %lu, interval: %" PRIu32 ")",
                    req.description, req.code,
                    (unsigned long)(CUSTOM_MILLIS - req.last_request_time), req.interval_ms);
            }
            continue;
        }

        // Send found request
        send_request(req.code, context);
        return;
    }

    // No more requests → end the cycle
    if (terminate_callback_) {
        terminate_callback_();
    }
}

bool RequestScheduler::process_response(uint8_t code, CN105Climate* context) {
    // Get context if not provided but callback is available
    if (!context && context_callback_) {
        context = context_callback_();
    }

    // Find out if the code is managed by the scheduler
    const InfoRequest* managed_request = nullptr;
    for (const auto& req : requests_) {
        if (req.code == code) {
            managed_request = &req;
            break;
        }
    }
    if (managed_request == nullptr) return false;

    // Only a request that is still awaiting may advance the cycle. Anything else is a
    // duplicate, or a reply to a request that was already given up on: ignore it.
    if (!managed_request->awaiting) {
        ESP_LOGD(LOG_CYCLE_TAG, "Ignoring reply for %s (0x%02X): not awaiting",
            managed_request->description, managed_request->code);
        return true;
    }

    mark_response_seen(code, context);
    send_next_after(code, context);
    return true;
}

void RequestScheduler::reset_pending() {
    for (auto& req : requests_) {
        req.awaiting = false;
    }
    current_request_index_ = -1;
}

void RequestScheduler::loop() {
    // Currently, timeouts are managed via callbacks
    // This method is intended for future management if necessary
    // (for example, to check timeouts manually in the main loop)
}
