#pragma once

#include "info_request.h"
#include <vector>
#include <functional>
#include <string>

namespace esphome {

    class CN105Climate; // forward declaration

    /**
     * @class RequestScheduler
     * @brief Manages the cyclical request queue and their sequence.
     *
     *This class extracts INFO request management logic from the CN105Climate component
     *to respect the single responsibility principle (SRP).
     */
    class RequestScheduler {
    public:
        /**
         * @brief Type of callback for sending a packet
         * @param code The code of the request to send
         */
        using SendCallback = std::function<void(uint8_t)>;

        /**
         * @brief Type of callback for timeout management
         * @param name Unique name of the timeout
         * @param timeout_ms Duration of the timeout in milliseconds
         * @param callback Function to call when timeout expires
         */
        using TimeoutCallback = std::function<void(const std::string&, uint32_t, std::function<void()>)>;

        /**
         * @brief Type of callback to end a cycle
         */
        using TerminateCallback = std::function<void()>;

        /**
         * @brief Type of callback to obtain the CN105Climate context (for canSend and onResponse)
         * @return Pointer to CN105Climate (can be nullptr)
         */
        using ContextCallback = std::function<CN105Climate* ()>;

        /**
         * @brief Constructor
         * @param send_callback Callback to send a packet
         * @param timeout_callback Callback to manage timeouts (can be nullptr if not used)
         * @param terminate_callback Callback to end a cycle
         * @param context_callback Callback to get the CN105Climate context (for canSend and onResponse)
         */
        RequestScheduler(
            SendCallback send_callback,
            TimeoutCallback timeout_callback = nullptr,
            TerminateCallback terminate_callback = nullptr,
            ContextCallback context_callback = nullptr
        );

        /**
         * @brief Saves a request to the queue
         * @param req The query to save (non-const reference to allow modification)
         */
        void register_request(InfoRequest& req);

        /**
         * @brief Empty the list of requests
         */
        void clear_requests();

        /**
         * @brief Disable a request by its code
         * @param code The request code to deactivate
         */
        void disable_request(uint8_t code);

        /**
         * @brief Enable a request by its code
         * @param code The request code to activate
         */
        void enable_request(uint8_t code);

        /**
         * @brief Bypass the interval timer of a request by its code
         * @param code The request code to bypass
         */
        void timer_bypass(uint8_t code);

        /**
         * @brief Check if the queue is empty
         * @return true if empty, false otherwise
         */
        bool is_empty() const;

        /**
         * @brief Send the next request after the one with the specified code
         * @param previous_code The code of the previous request (0x00 to start)
         * @param context CN105Climate context to check canSend (can be nullptr, uses context_callback_ if provided)
         */
        void send_next_after(uint8_t previous_code, CN105Climate* context = nullptr);

        /**
         * @brief Marks a response as received for a given code and calls the onResponse callback if present
         * @param code The code of the request whose response was received
         * @param context CN105Climate context to call onResponse (can be nullptr)
         */
        void mark_response_seen(uint8_t code, CN105Climate* context = nullptr);

        /**
         * @brief Processes a response received
         * @param code The code of the response received
         * @param context CN105Climate context to call onResponse (can be nullptr, uses context_callback_ if provided)
         * @return true if the response has been processed, false otherwise
         */
        bool process_response(uint8_t code, CN105Climate* context = nullptr);

        /**
         * @brief Method to call in the main loop to manage timeouts
         *Note: Timeout management is currently managed via callbacks,
         *this method is intended for future management if necessary.
         */
        void loop();

    private:
        std::vector<InfoRequest> requests_;         // Requests queue
        int current_request_index_;                 // Index of the current request
        SendCallback send_callback_;                // Callback to send a packet
        TimeoutCallback timeout_callback_;          // Callback to manage timeouts
        TerminateCallback terminate_callback_;      // Callback to end a cycle
        ContextCallback context_callback_;          // Callback to get the CN105Climate context

        /**
         * @brief Sends a specific request by its code
         * @param code The code of the request to send
         * @param context CN105Climate context to check canSend (can be nullptr)
         */
        void send_request(uint8_t code, CN105Climate* context = nullptr);
    };

}

