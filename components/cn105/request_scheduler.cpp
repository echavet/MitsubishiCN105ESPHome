#include "request_scheduler.h"
#include "Globals.h"
#include "cn105.h"
#include <esphome.h>

using namespace esphome;

RequestScheduler::RequestScheduler(
    SendCallback send_callback,
    TimeoutCallback timeout_callback,
    TerminateCallback terminate_callback,
    ContextCallback context_callback
) : current_request_index_(-1),
send_callback_(send_callback),
timeout_callback_(timeout_callback),
terminate_callback_(terminate_callback),
context_callback_(context_callback) {}

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

bool RequestScheduler::is_empty() const {
    return requests_.empty();
}

void RequestScheduler::send_request(uint8_t code, CN105Climate* context) {
    // Obtenir le contexte si non fourni mais que le callback est disponible
    if (!context && context_callback_) {
        context = context_callback_();
    }

    for (size_t i = 0; i < requests_.size(); ++i) {
        auto& req = requests_[i];
        if (req.code != code) continue;
        if (req.disabled) { return; }

        // Vérifier canSend si présent et si le contexte est disponible
        if (req.canSend && context) {
            if (!req.canSend(*context)) {
                return;
            }
        }

        const char* tag = req.log_tag ? req.log_tag : LOG_CYCLE_TAG;
        ESP_LOGD(tag, "Sending %s (0x%02X)", req.description, req.code);

        req.awaiting = true;
        req.last_request_time = CUSTOM_MILLIS;

        // Envoyer le paquet via le callback
        if (send_callback_) {
            send_callback_(req.code);
        }

        // Gérer le timeout si configuré et si le callback est disponible
        if (req.soft_timeout_ms > 0 && timeout_callback_) {
            uint8_t code_copy = req.code;
            const std::string tname = req.timeout_name.empty() ?
                (std::string("info_timeout_") + std::to_string(code_copy)) :
                req.timeout_name;

            timeout_callback_(tname, req.soft_timeout_ms, [this, code_copy]() {
                // Obtenir le contexte pour send_next_after
                CN105Climate* ctx = nullptr;
                if (this->context_callback_) {
                    ctx = this->context_callback_();
                }

                // Si la réponse est toujours attendue, considérer comme un échec soft et continuer
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
    // Obtenir le contexte si non fourni mais que le callback est disponible
    if (!context && context_callback_) {
        context = context_callback_();
    }

    for (auto& req : requests_) {
        if (req.code == code) {
            req.awaiting = false;
            req.failures = 0;
            ESP_LOGD(LOG_CYCLE_TAG, "Received %s <0x%02X>", req.description, req.code);

            // Appeler le callback onResponse si présent et si le contexte est disponible
            if (req.onResponse && context) {
                req.onResponse(*context);
            }
            return;
        }
    }
}

void RequestScheduler::send_next_after(uint8_t previous_code, CN105Climate* context) {
    // Obtenir le contexte si non fourni mais que le callback est disponible
    if (!context && context_callback_) {
        context = context_callback_();
    }

    // Trouver l'index de départ (par code) puis essayer les entrées activables suivantes dans l'ordre
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

        // Vérifier canSend si présent et si le contexte est disponible
        if (req.canSend && context) {
            if (!req.canSend(*context)) {
                if (req.log_tag) {
                    ESP_LOGD(req.log_tag, "Skipping %s (0x%02X): canSend returned false", req.description, req.code);
                }
                continue;
            }
        }

        if (req.interval_ms > 0 && (CUSTOM_MILLIS - req.last_request_time < req.interval_ms)) {
            if (req.log_tag) {
                ESP_LOGD(req.log_tag, "Skipping %s (0x%02X) - interval not elapsed (elapsed: %lu, interval: %u)",
                    req.description, req.code,
                    (unsigned long)(CUSTOM_MILLIS - req.last_request_time), req.interval_ms);
            }
            continue;
        }

        // Envoyer la requête trouvée
        send_request(req.code, context);
        return;
    }

    // Plus de requêtes → terminer le cycle
    if (terminate_callback_) {
        terminate_callback_();
    }
}

bool RequestScheduler::process_response(uint8_t code, CN105Climate* context) {
    // Obtenir le contexte si non fourni mais que le callback est disponible
    if (!context && context_callback_) {
        context = context_callback_();
    }

    // Chercher si le code est géré par le scheduler
    bool handled = false;
    for (auto& req : requests_) {
        if (req.code == code) {
            handled = true;
            break;
        }
    }
    if (!handled) return false;

    mark_response_seen(code, context);
    send_next_after(code, context);
    return true;
}

void RequestScheduler::loop() {
    // Actuellement, la gestion des timeouts est faite via des callbacks
    // Cette méthode est prévue pour une gestion future si nécessaire
    // (par exemple, pour vérifier les timeouts manuellement dans le loop principal)
}

