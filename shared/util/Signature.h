#ifndef SHARED_UTIL_SIGNATURE_H
#define SHARED_UTIL_SIGNATURE_H

#include "SSLHelpers.h"

#include <Logging.h>
#include <io/Format.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include <openssl/bio.h>
#include <openssl/evp.h>

namespace util {
class Signature {
    public:
        /**
         * Calculates a signature (using an SHA-256 digest) over the provided buffer.
         */
        static void sign(EVP_PKEY *key, const void *data, const size_t dataLen, std::vector<std::byte> &out) {
            int err;
            size_t digestLen = 0;
            EVP_MD_CTX *mdctx = NULL;

            XASSERT(data && dataLen, "Invalid data");

            // set up context with SHA-256
            mdctx = EVP_MD_CTX_create();
            XASSERT(mdctx, "Failed to create signature context");

            err = EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, key);
            if(err != 1) {
                throw std::runtime_error(f("Failed to init digest: {}", SSLHelpers::getErrorStr()));
            }

            // squelch in the message and then finalize
            err = EVP_DigestSignUpdate(mdctx, data, dataLen);
            if(err != 1) {
                throw std::runtime_error(f("Failed to update digest: {}", SSLHelpers::getErrorStr()));
            }

            // copy out
            err = EVP_DigestSignFinal(mdctx, nullptr, &digestLen);
            if(err != 1) {
                throw std::runtime_error(f("Failed to get digest length: {}", SSLHelpers::getErrorStr()));
            }

            XASSERT(digestLen, "Invalid digest length");

            out.resize(digestLen);

            err = EVP_DigestSignFinal(mdctx, reinterpret_cast<unsigned char *>(out.data()), &digestLen);
            if(err != 1) {
                throw std::runtime_error(f("Failed to get digest: {}", SSLHelpers::getErrorStr()));
            }

            out.resize(digestLen);

            // clean up
            EVP_MD_CTX_destroy(mdctx);
        }

        /**
         * Validates the signature over the provided buffer.
         */
        static bool verify(EVP_PKEY *key, const void *data, const size_t dataLen, const std::vector<std::byte> &signature) {
            int err;
            EVP_MD_CTX *mdctx = nullptr;

            XASSERT(data && dataLen && !signature.empty(), "Invalid data");

            // set up context with SHA-256
            mdctx = EVP_MD_CTX_create();
            XASSERT(mdctx, "Failed to create signature context");

            err = EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, key);
            if(err != 1) {
                throw std::runtime_error(f("Failed to init digest verify: {}", SSLHelpers::getErrorStr()));
            }

            // load in the message and verify
            err = EVP_DigestVerifyUpdate(mdctx, data, dataLen);
            if(err != 1) {
                throw std::runtime_error(f("Failed to update verify digest: {}", SSLHelpers::getErrorStr()));
            }

            err = EVP_DigestVerifyFinal(mdctx,
                    reinterpret_cast<const unsigned char *>(signature.data()), signature.size());

            // clean up
            EVP_MD_CTX_destroy(mdctx);

            if(err <= -1) {
                throw std::runtime_error(f("Failed to verify signature {}: {}", err, SSLHelpers::getErrorStr()));
            }

            return (err == 1);
        }
};
}

#endif
