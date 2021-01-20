#include "AuthManager.h"

#include "io/PrefsManager.h"
#include "util/SSLHelpers.h"
#include "util/REST.h"

#include "io/Format.h"
#include <Logging.h>

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/types/string.hpp>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/pem.h>

#include <curl/curl.h>
#include <jwt-cpp/jwt.h>

using namespace web;

/**
 * The key data stored in preferences is encrypted using AES-128, with a fixed key and IV common
 * to all instances of the app. This is really just to protect the data stored in the prefs
 * store from prying eyes than any sort of security against attackers.
 */
const std::array<uint8_t, 16> AuthManager::kAuthDataKey = {
    0x82, 0xce, 0x43, 0x25, 0xb6, 0xc1, 0xda, 0x2f,
    0x03, 0xc6, 0x6b, 0xb3, 0xa4, 0x98, 0xe1, 0xe1
};
const std::array<uint8_t, 16> AuthManager::kAuthDataIV = {
    0xde, 0x37, 0x84, 0x30, 0xa0, 0xce, 0xcc, 0xe0,
    0x8f, 0x33, 0xff, 0x2a, 0x24, 0xb8, 0xe4, 0xa7
};

const std::string AuthManager::kAuthDataPrefsKey = "auth.data.key";
const std::string AuthManager::kPlayerIdPrefsKey = "player.id";


AuthManager *AuthManager::gShared = nullptr;

/**
 * Initializes the auth manager.
 */
AuthManager::AuthManager() {
    // cURL init
    curl_global_init(CURL_GLOBAL_ALL);

    // try to load player id, keys
    auto id = io::PrefsManager::getUuid(kPlayerIdPrefsKey);
    if(!id) {
        // generate seeds for a UUID random generator
        std::random_device rand;
        auto seedData = std::array<int, std::mt19937::state_size> {};
        std::generate(std::begin(seedData), std::end(seedData), std::ref(rand));

        std::seed_seq seq(std::begin(seedData), std::end(seedData));

        // create a random player id
        std::mt19937 generator(seq);
        uuids::uuid_random_generator gen{generator};
        const uuids::uuid newId = gen();

        // save it to prefs and set our value
        io::PrefsManager::setUuid(kPlayerIdPrefsKey, newId);
        this->playerId = newId;

        Logging::info("Generated new player id: {}", newId);
    } else {
        this->playerId = *id;
    }

    this->loadKeys();

    // set up the REST handler
#ifdef NDEBUG
    #error "TODO: define API endpoint for prod"
#else
    this->api = new util::REST("http://cubeland-api.test");
#endif
}

/**
 * Cleans up auth manager resources.
 */
AuthManager::~AuthManager() {
    if(this->key) {
        EVP_PKEY_free(this->key);
    }

    delete this->api;
}



/**
 * Attempts to load the authentication keypair from the preferences store.
 *
 * @return Whether key data was successfully loaded.
 */
bool AuthManager::loadKeys() {
    int err;

    auto blob = io::PrefsManager::getBlob(kAuthDataPrefsKey);
    if(!blob) return false;

    // try to decrypt and decode it
    std::vector<uint8_t> decrypted;
    decrypt(*blob, decrypted);

    try {
        std::stringstream stream(std::string(decrypted.begin(), decrypted.end()));
        cereal::PortableBinaryInputArchive arc(stream);

        AuthData d;
        arc(d);

        this->authKeys = std::move(d);
    } catch(std::exception &e) {
        Logging::error("Failed to load auth data: {}", e.what());
        return false;
    }

    // decode private key
    BIO *bio = BIO_new(BIO_s_mem());
    XASSERT(bio, "Failed to allocate mem BIO");

    auto readPtr = this->authKeys->pemPrivate.data();
    size_t toWrite = this->authKeys->pemPrivate.size();

    while(toWrite > 0) {
        err = BIO_write(bio, readPtr, toWrite);

        if(err <= 0) {
            throw std::runtime_error(f("BIO_write failed: {}", err));
        }

        toWrite -= err;
        readPtr += err;
    }

    this->key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    XASSERT(this->key, "Failed to load private key: {}", util::SSLHelpers::getErrorStr());

    // clean up
    BIO_free(bio);

    return true;
}

/**
 * Saves the key data to the preferences store.
 */
void AuthManager::saveKeys() {
    // save key data
    if(this->authKeys.has_value()) {
        // serialize the data
        std::stringstream stream;
        cereal::PortableBinaryOutputArchive arc(stream);

        arc(*this->authKeys);

        const auto str = stream.str();
        std::vector<uint8_t> plain(str.begin(), str.end());

        // encrypt it, then store this in prefs
        std::vector<uint8_t> encrypted;
        encrypt(plain, encrypted);

        io::PrefsManager::setBlob(kAuthDataPrefsKey, encrypted);
    }
    // otherwise, clear the prefs key
    else {
        /// XXX: we ought to really erase it rather than just overwrite it with zero byte data
        io::PrefsManager::deleteBlob(kAuthDataPrefsKey);
    }
}



/**
 * Generates a new authentication keypair.
 *
 * The keypair uses the brainpoolP384t1 curve. This is probably good enough for what we're doing,
 * even though it's not one of those "safe" curves everyone talks about. At least it's not a NIST
 * curve...
 */
void AuthManager::generateKeys() {
    int err;
    EC_KEY *curve = nullptr;
    EVP_PKEY *pkey = nullptr;
    char *buf = nullptr;

    // select our curve
    err = OBJ_txt2nid("brainpoolP384t1");
    XASSERT(err != NID_undef, "Failed to find ECDSA curve");

    curve = EC_KEY_new_by_curve_name(err);
    XASSERT(curve, "Failed to create ECDSA curve: {}", util::SSLHelpers::getErrorStr());

    EC_KEY_set_asn1_flag(curve, OPENSSL_EC_NAMED_CURVE);

    // generate the keypair
    err = EC_KEY_generate_key(curve);
    XASSERT(err == 1, "Failed to generate ECDSA key: {}", util::SSLHelpers::getErrorStr());

    pkey = EVP_PKEY_new();

    err = EVP_PKEY_assign_EC_KEY(pkey, curve);
    XASSERT(err == 1, "Failed to assign ECDSA key: {}", util::SSLHelpers::getErrorStr());

    // extract the private/public key as PEM format
    BIO *bio = BIO_new(BIO_s_mem());

    err = PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    XASSERT(err == 1, "Failed to get ECDSA private key: {}", util::SSLHelpers::getErrorStr());
    err = BIO_get_mem_data(bio, &buf);
    XASSERT(err, "Failed to get private key data");
    std::string privPem(buf, err);

    BIO_reset(bio);
    err = PEM_write_bio_PUBKEY(bio, pkey);
    XASSERT(err == 1, "Failed to get ECDSA public key: {}", util::SSLHelpers::getErrorStr());
    err = BIO_get_mem_data(bio, &buf);
    XASSERT(err, "Failed to get public key data");
    std::string pubPem(buf, err);

    // clean up
    // need not free key as EVP_PKEY has taken ownership of it
    if(this->key) {
        EVP_PKEY_free(this->key);
        this->key = pkey;
    }
    BIO_free(bio);

    // build the auth data
    AuthData d;
    d.pemPrivate = privPem;
    d.pemPublic = pubPem;
    d.generated = std::chrono::system_clock::now();

    this->authKeys = d;
}



/**
 * Encrypts the given data buffer using the auth data key.
 */
void AuthManager::encrypt(const std::vector<uint8_t> &in, std::vector<uint8_t> &out) {
    int err;
    int outlen1 = 0, outlen2 = 0;

    // ensure we have space for one extra block
    out.resize(ceil(((float) in.size()) / 16.) * 16 + 16);

    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX_init(&ctx);

    err = EVP_EncryptInit_ex(&ctx, EVP_aes_128_cbc(), nullptr, kAuthDataKey.data(),
            kAuthDataIV.data());
    XASSERT(err == 1, "EVP_EncryptInit_ex() failed: {}", util::SSLHelpers::getErrorStr());

    err = EVP_EncryptUpdate(&ctx, out.data(), &outlen1, in.data(), in.size());
    XASSERT(err == 1, "EVP_EncryptUpdate() failed: {}", util::SSLHelpers::getErrorStr());

    err = EVP_EncryptFinal_ex(&ctx, out.data() + outlen1, &outlen2);
    XASSERT(err == 1, "EVP_EncryptFinal_ex() failed: {}", util::SSLHelpers::getErrorStr());

    out.resize(outlen1 + outlen2);
    EVP_CIPHER_CTX_cleanup(&ctx);
}

/**
 * Decrypts the given data buffer using the auth data key.
 */
void AuthManager::decrypt(const std::vector<uint8_t> &in, std::vector<uint8_t> &out) {
    int err;
    int outlen1 = 0, outlen2 = 0;

    out.resize(ceil(((float) in.size()) / 16.) * 16 + 16);

    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX_init(&ctx);

    err = EVP_DecryptInit_ex(&ctx, EVP_aes_128_cbc(), nullptr, kAuthDataKey.data(),
            kAuthDataIV.data());
    XASSERT(err == 1, "EVP_DecryptInit_ex() failed: {}", util::SSLHelpers::getErrorStr());

    err = EVP_DecryptUpdate(&ctx, out.data(), &outlen1, in.data(), in.size());
    XASSERT(err == 1, "EVP_DecryptUpdate() failed: {}", util::SSLHelpers::getErrorStr());

    err = EVP_DecryptFinal_ex(&ctx, out.data() + outlen1, &outlen2);
    XASSERT(err == 1, "EVP_DecryptFinal_ex() failed: {}", util::SSLHelpers::getErrorStr());

    out.resize(outlen1 + outlen2);
    EVP_CIPHER_CTX_cleanup(&ctx);
}

/**
 * Signs the given data using the private key we loaded earlier or generated.
 */
void AuthManager::signData(const void *data, const size_t dataLen, std::vector<std::byte> &out) {
    int err;
    size_t digestLen = 0;
    EVP_MD_CTX *mdctx = NULL;

    XASSERT(data && dataLen, "Invalid data");

    // set up context with SHA-256
    mdctx = EVP_MD_CTX_create();
    XASSERT(mdctx, "Failed to create signature context");

    err = EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, this->key);
    XASSERT(err == 1, "Failed to init digest ctx: {}", util::SSLHelpers::getErrorStr());

    // squelch in the message and then finalize
    err = EVP_DigestSignUpdate(mdctx, data, dataLen);
    XASSERT(err == 1, "Failed to update digest: {}", util::SSLHelpers::getErrorStr());

    // copy out
    err = EVP_DigestSignFinal(mdctx, nullptr, &digestLen);
    XASSERT(err == 1, "Failed to finalize digest: {}", util::SSLHelpers::getErrorStr());

    XASSERT(digestLen, "Invalid digest length");

    out.resize(digestLen);

    err = EVP_DigestSignFinal(mdctx, reinterpret_cast<unsigned char *>(out.data()), &digestLen);
    XASSERT(err == 1, "Failed to copy digest: {}", util::SSLHelpers::getErrorStr());

    // clean up
    EVP_MD_CTX_destroy(mdctx);
}



/**
 * Build up a REST request that registers the player keys.
 */
void AuthManager::restRegisterKeys() {
    using namespace rapidjson;

    // build the request body
    const auto idString = uuids::to_string(this->playerId);
    const auto pubString = this->authKeys->pemPublic;

    Document body;
    body.SetObject()
        .AddMember("playerId", StringRef(idString.c_str(), idString.size()), body.GetAllocator())
        .AddMember("pubKey", StringRef(pubString.c_str(), pubString.size()), body.GetAllocator());

    // submit it and parse response
    Document response;

    this->api->request("/user/register", body, response, false);

    // interpret response
    if(!response["success"].GetBool()) {
        throw std::runtime_error("Failed to register user");
    }

    Logging::debug("Server user id: {}", response["id"].GetUint());
}
