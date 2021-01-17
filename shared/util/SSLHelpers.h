#ifndef SHARED_UTIL_SSLHELPERS_H
#define SHARED_UTIL_SSLHELPERS_H

#include <string>

#include <openssl/bio.h>
#include <openssl/err.h>

namespace util {
class SSLHelpers {
    public:
        /**
         * Gets the current thread's SSL library error string.
         */
        static const std::string getErrorStr() {
            // print errors into a memory buffer and extract it
            BIO *bio = BIO_new(BIO_s_mem());
            ERR_print_errors(bio);

            char *buf = nullptr;
            const auto length = BIO_get_mem_data(bio, &buf);

            // create C++ string and clean up
            const auto str = std::string(buf, length);

            BIO_free(bio);
            return str;
        }
};
}

#endif
