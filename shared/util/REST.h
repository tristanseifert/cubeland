/**
 * Helpers to make REST requests using the cURL library
 */
#ifndef SHARED_UTIL_REST_H
#define SHARED_UTIL_REST_H

#include <cstddef>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "io/Format.h"

#include <version.h>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace util {
class REST {
    private:
        /**
         * Write (e.g. data received from server) callback. It simply appends to the given vector.
         */
        static size_t writeCb(void *_ptr, size_t size, size_t nmemb, void *userdata) {
            auto vec = reinterpret_cast<std::vector<unsigned char> *>(userdata);
            auto readPtr = reinterpret_cast<const unsigned char *>(_ptr);

            vec->insert(vec->end(), readPtr, readPtr + (size * nmemb));
            return (size * nmemb);
        }

        /**
         * Info for the read callback
         */
        struct ReadCbInfo {
            /// current offset into body
            size_t offset = 0;
            /// data to write
            std::string body;
        };

        /**
         * cURL body upload function. The userdata argument is a pointer to an std::string that
         * should be sent as the body.
         */
        static size_t readCb(void *ptr, size_t size, size_t nmemb, void *userdata) {
            // bail if no body required
            const auto info = reinterpret_cast<ReadCbInfo *>(userdata);
            if(!info) {
                return 0;
            }

            // if we've copied all of the data, bail
            if(info->offset >= info->body.size()) {
                return 0;
            }

            // copy out
            const size_t bytesRequested = size * nmemb;
            const size_t bytesToCopy = std::min(bytesRequested, info->body.size() - info->offset);

            memcpy(ptr, info->body.data() + info->offset, bytesToCopy);
            info->offset += bytesToCopy;

            return bytesToCopy;
        }

    public:
        enum class RequestMethod {
            GET,
            POST,
            DELETE,
            PUT,
        };

    public:
        /**
         * Creates a new REST API helper with the given API base.
         *
         * @param base Base URL for the API. Must not have a trailing slash.
         */
        REST(const std::string &baseUrl) : base(baseUrl) {
            this->curl = curl_easy_init();
        }

        /**
         * Cleans up REST worker resources.
         */
        ~REST() {
            curl_easy_cleanup(this->curl);
        }

        /**
         * Makes a synchronous GET request against the given API endpoint.
         */
        void request(const std::string &path, rapidjson::Document &response,
                const bool authorize = true) {
            rapidjson::Document fakeBody;
            this->request(path, fakeBody, response, authorize, RequestMethod::GET);
        }

        /**
         * Makes a synchronous request to the given API endpoint.
         *
         * If we have a valid API token, we can automatically add it to the request as part of the
         * Authorization header value. The request body is JSON encoded.
         *
         * @throws For network and server/API (4xx, 5xx) errors.
         */
        void request(const std::string &path, rapidjson::Document &body,
                rapidjson::Document &response, const bool authorize = true,
                const RequestMethod method = RequestMethod::POST) {
            ReadCbInfo read;

            // request url
            curl_easy_setopt(this->curl, CURLOPT_URL, (this->base + path).c_str());
            curl_easy_setopt(this->curl, CURLOPT_FAILONERROR, 1);

            // set the user agent and authorization headers
            struct curl_slist *headers = nullptr;

#ifdef __APPLE__
            const static std::string kPlatform = "MacOS";
#else
            const static std::string kPlatform = "unk";
#endif

            const auto ua = f("User-Agent: Cubeland/{} {}", kPlatform, gVERSION_TAG);
            headers = curl_slist_append(headers, ua.c_str());

            if(authorize) {
                const auto token = web::AuthManager::apiAuthToken();
                if(token) {
                    const auto hdr = f("Authorization: Bearer {}", *token);
                    headers = curl_slist_append(headers, hdr.c_str());
                }
            }

            // request type
            switch(method) {
                case RequestMethod::POST:
                    curl_easy_setopt(this->curl, CURLOPT_POST, 1);
                    break;
                case RequestMethod::PUT:
                    curl_easy_setopt(this->curl, CURLOPT_POST, 1);
                    curl_easy_setopt(this->curl, CURLOPT_CUSTOMREQUEST, "PUT");
                    break;
                case RequestMethod::DELETE:
                    curl_easy_setopt(this->curl, CURLOPT_POST, 1);
                    curl_easy_setopt(this->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
                    break;

                case RequestMethod::GET:
                    curl_easy_setopt(this->curl, CURLOPT_HTTPGET, 1);
                    break;
            }

            // serialize body for all requests that aren't GET type
            if(method != RequestMethod::GET) {
                using namespace rapidjson;

                // stringify it
                StringBuffer buffer;
                Writer<StringBuffer> writer(buffer);
                body.Accept(writer);

                read.body = buffer.GetString();

                curl_easy_setopt(this->curl, CURLOPT_READFUNCTION, &REST::readCb);
                curl_easy_setopt(this->curl, CURLOPT_READDATA, &read);
                curl_easy_setopt(this->curl, CURLOPT_POSTFIELDSIZE_LARGE, read.body.size());

                // set content type to be JSON
                headers = curl_slist_append(headers, "Content-Type: application/json");
            } else {
                curl_easy_setopt(this->curl, CURLOPT_READDATA, nullptr);
                curl_easy_setopt(this->curl, CURLOPT_POSTFIELDSIZE_LARGE, 0);
            }

            // buffer to receive data into
            std::vector<unsigned char> rxBuf;

            curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, &REST::writeCb);
            curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &rxBuf);

            // finish preparing the request and send it
            curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, headers);

            const auto ret = curl_easy_perform(this->curl);

            // clean up
            curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, nullptr);
            curl_slist_free_all(headers);

            // handle response code
            if(ret != CURLE_OK) {
                throw std::runtime_error(f("cURL error: {}", curl_easy_strerror(ret)));
            }

            // parse body
            if(!rxBuf.empty()) {
                using namespace rapidjson;

                const std::string body(rxBuf.begin(), rxBuf.end());
                StringStream stream(body.c_str());

                response.ParseStream(stream);
            }
        }

    private:
        /// cURL session
        CURL *curl = nullptr;

        /// base URL of the API
        const std::string base;
};
}

#endif
