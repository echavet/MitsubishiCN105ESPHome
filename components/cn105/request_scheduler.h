#pragma once

#include "info_request.h"
#include <vector>
#include <functional>
#include <string>

namespace esphome {

    class CN105Climate; // forward declaration

    /**
     * @class RequestScheduler
     * @brief Gère la file d'attente des requêtes cycliques et leur enchaînement.
     *
     * Cette classe extrait la logique de gestion des requêtes INFO du composant CN105Climate
     * pour respecter le principe de responsabilité unique (SRP).
     */
    class RequestScheduler {
    public:
        /**
         * @brief Type de callback pour l'envoi d'un paquet
         * @param code Le code de la requête à envoyer
         */
        using SendCallback = std::function<void(uint8_t)>;

        /**
         * @brief Type de callback pour la gestion des timeouts
         * @param name Nom unique du timeout
         * @param timeout_ms Durée du timeout en millisecondes
         * @param callback Fonction à appeler à l'expiration du timeout
         */
        using TimeoutCallback = std::function<void(const std::string&, uint32_t, std::function<void()>)>;

        /**
         * @brief Type de callback pour terminer un cycle
         */
        using TerminateCallback = std::function<void()>;

        /**
         * @brief Type de callback pour obtenir le contexte CN105Climate (pour canSend et onResponse)
         * @return Pointeur vers CN105Climate (peut être nullptr)
         */
        using ContextCallback = std::function<CN105Climate* ()>;

        /**
         * @brief Constructeur
         * @param send_callback Callback pour envoyer un paquet
         * @param timeout_callback Callback pour gérer les timeouts (peut être nullptr si non utilisé)
         * @param terminate_callback Callback pour terminer un cycle
         * @param context_callback Callback pour obtenir le contexte CN105Climate (pour canSend et onResponse)
         */
        RequestScheduler(
            SendCallback send_callback,
            TimeoutCallback timeout_callback = nullptr,
            TerminateCallback terminate_callback = nullptr,
            ContextCallback context_callback = nullptr
        );

        /**
         * @brief Enregistre une requête dans la file d'attente
         * @param req La requête à enregistrer (référence non-const pour permettre modification)
         */
        void register_request(InfoRequest& req);

        /**
         * @brief Vide la liste des requêtes
         */
        void clear_requests();

        /**
         * @brief Désactive une requête par son code
         * @param code Le code de la requête à désactiver
         */
        void disable_request(uint8_t code);

        /**
         * @brief Vérifie si la file d'attente est vide
         * @return true si vide, false sinon
         */
        bool is_empty() const;

        /**
         * @brief Envoie la prochaine requête après celle avec le code spécifié
         * @param previous_code Le code de la requête précédente (0x00 pour démarrer)
         * @param context Contexte CN105Climate pour vérifier canSend (peut être nullptr, utilise context_callback_ si fourni)
         */
        void send_next_after(uint8_t previous_code, CN105Climate* context = nullptr);

        /**
         * @brief Marque une réponse comme reçue pour un code donné et appelle le callback onResponse si présent
         * @param code Le code de la requête dont la réponse a été reçue
         * @param context Contexte CN105Climate pour appeler onResponse (peut être nullptr)
         */
        void mark_response_seen(uint8_t code, CN105Climate* context = nullptr);

        /**
         * @brief Traite une réponse reçue
         * @param code Le code de la réponse reçue
         * @param context Contexte CN105Climate pour appeler onResponse (peut être nullptr, utilise context_callback_ si fourni)
         * @return true si la réponse a été traitée, false sinon
         */
        bool process_response(uint8_t code, CN105Climate* context = nullptr);

        /**
         * @brief Méthode à appeler dans le loop principal pour gérer les timeouts
         * Note: La gestion des timeouts est actuellement gérée via des callbacks,
         * cette méthode est prévue pour une gestion future si nécessaire.
         */
        void loop();

    private:
        std::vector<InfoRequest> requests_;          // File d'attente des requêtes
        int current_request_index_;                  // Index de la requête courante
        SendCallback send_callback_;                  // Callback pour envoyer un paquet
        TimeoutCallback timeout_callback_;            // Callback pour gérer les timeouts
        TerminateCallback terminate_callback_;        // Callback pour terminer un cycle
        ContextCallback context_callback_;            // Callback pour obtenir le contexte CN105Climate

        /**
         * @brief Envoie une requête spécifique par son code
         * @param code Le code de la requête à envoyer
         * @param context Contexte CN105Climate pour vérifier canSend (peut être nullptr)
         */
        void send_request(uint8_t code, CN105Climate* context = nullptr);
    };

}

