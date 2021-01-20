/**
 * Handles storing the client keypair and authentication information, as well as interacting with
 * the REST API  for authentication.
 */
#ifndef WEB_AUTHMANAGER_H
#define WEB_AUTHMANAGER_H

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <uuid.h>

struct evp_pkey_st; 

namespace util {
class REST;
}

namespace web {
class AuthManager {
    public:
        static void init() {
            gShared = new AuthManager;
        }
        static void shutdown() {
            delete gShared;
            gShared = nullptr;
        }

        /// Whether authentication keys are available
        static const bool areKeysAvailable() {
            return gShared->authKeys.has_value();
        }

        /// Returns the player ID
        static const uuids::uuid getPlayerId() {
            return gShared->playerId;
        }

        /// Generates a new keypair.
        static void generateAuthKeys(const bool save = true) {
            gShared->generateKeys();
            if(save) gShared->saveKeys();
        }
        /// Clears existing auth data
        static void clearAuthKeys(const bool save = true) {
            gShared->authKeys = std::nullopt;
            if(save) gShared->saveKeys();
        }


        /// Registers with the API the current auth keys.
        static void registerAuthKeys(const bool save = true) {
            gShared->restRegisterKeys();
            gShared->saveKeys();
        }

        /// Returns the authorization token if available
        static std::optional<std::string> apiAuthToken() {
            return std::nullopt;
        }

    private:
        /// AES-128 key for encrypting the auth data
        static const std::array<uint8_t, 16> kAuthDataKey;
        /// Initialization vector for auth data encryption
        static const std::array<uint8_t, 16> kAuthDataIV;

        static void encrypt(const std::vector<uint8_t> &in, std::vector<uint8_t> &out);
        static void decrypt(const std::vector<uint8_t> &in, std::vector<uint8_t> &out);

    private:
        static const std::string kPlayerIdPrefsKey;
        static const std::string kAuthDataPrefsKey;

        /**
         * Authentication data (public/private key pair) stored in user preferences. This data will
         * be encrypted as well.
         */
        struct AuthData {
            /// PEM-encoded private key
            std::string pemPrivate;
            /// PEM-encoded public key
            std::string pemPublic;

            /// date at which the key was generated
            std::chrono::system_clock::time_point generated;

            template<class Archive> void serialize(Archive &arc) {
                arc(this->pemPrivate);
                arc(this->pemPublic);
                arc(this->generated);
            }
        };

    private:
        AuthManager();
        ~AuthManager();

        bool loadKeys();
        void saveKeys();

        void generateKeys();

        void restRegisterKeys();

        void signData(const std::vector<std::byte> &data, std::vector<std::byte> &out) {
            this->signData(data.data(), data.size(), out);
        }
        void signData(const void *data, const size_t dataLen, std::vector<std::byte> &out);

    private:
        static AuthManager *gShared;

    private:
        /// Player ID
        uuids::uuid playerId;
        /// Loaded auth keys, if any
        std::optional<AuthData> authKeys = std::nullopt;

        /// REST API interface
        util::REST *api;

        /// loaded private/public keys 
        evp_pkey_st *key = nullptr;
};
}

#endif
