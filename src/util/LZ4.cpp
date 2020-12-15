#include "LZ4.h"
#include "io/Format.h"
#include <Logging.h>

#include <lz4.h>
#include <lz4frame.h>

#include <cstring>
#include <stdexcept>

using namespace util;

/**
 * Sets up the compression and decompression contexts.
 */
LZ4::LZ4() {
    size_t err;

    // allocate decompression context 
    err = LZ4F_createDecompressionContext(&this->dctx, LZ4F_VERSION);
    if(LZ4F_isError(err)) {
        throw std::runtime_error(f("failed to create LZ4F decompression context: {} ({:x})", 
                    LZ4F_getErrorName(err), err));
    }
}
/**
 * Releases the LZ4 contexts.
 */
LZ4::~LZ4() {
    size_t err;

    // decompression context first
    if(this->dctx) {
        err = LZ4F_freeDecompressionContext(this->dctx);
        XASSERT(!err, "failed to release LZ4F decompression context: {} ({:x})",
                LZ4F_getErrorName(err), err);
    }
}

/**
 * Compresses the data in the input buffer to the output buffer.
 *
 * The output buffer will be resized after compression to be the precise size of the compressed
 * text. It's resized to at least LZ4_compressBound(in.size()) before starting if needed.
 */
void LZ4::compress(const void *in, const size_t inLen, std::vector<char> &out) {
    size_t err;

    // bail if input is zero bytes
    if(!inLen) {
        out.clear();
        return;
    }

    // resize output buffer if needed
    const auto minOutCapacity = LZ4F_compressFrameBound(inLen, nullptr);
    if(out.size() < minOutCapacity) {
        out.resize(minOutCapacity);
    }

    // set up the LZ4 prefs struct (this contains the content size value)
    LZ4F_preferences_t prefs;
    memset(&prefs, 0, sizeof(prefs));

    prefs.frameInfo.contentSize = inLen;

    // perform compression
    err = LZ4F_compressFrame(out.data(), out.size(), in, inLen, &prefs);
    if(LZ4F_isError(err)) {
        throw std::runtime_error(f("LZ4F_compressFrame() failed: {} ({:x})", 
                    LZ4F_getErrorName(err), err));
    }

    // truncate the output buffer
    out.resize(err);
}

/**
 * Decompresses the data into the given output buffer.
 *
 * @return Whether all data was decompressed; if false, some (or none) of the data may have been
 * decompressed successfully.
 */ 
bool LZ4::decompress(const std::vector<char> &in, std::vector<char> &out) {
    size_t err, consumedSrc = 0, consumedDst = 0;
    LZ4F_frameInfo_t info;
    memset(&info, 0, sizeof(info));

    // bail if input is empty
    if(in.empty()) {
        out.clear();
        return true;
    }

    // reset the context
    LZ4F_resetDecompressionContext(this->dctx);

    // try to read the frame parameters (get uncompressed size)
    if(in.size() < LZ4F_HEADER_SIZE_MAX) {
        throw std::runtime_error("Insufficient space for LZ4 header");
    }

    consumedSrc = in.size();
    err = LZ4F_getFrameInfo(this->dctx, &info, in.data(), &consumedSrc);
    if(LZ4F_isError(err)) {
        throw std::runtime_error(f("LZ4F_getFrameInfo() failed: {} ({:x})", 
                    LZ4F_getErrorName(err), err));
    }
    XASSERT(info.contentSize < 1024 * 1024 * 128, "decompressed size too big: {}", info.contentSize);

    out.resize(info.contentSize);

    // decode until done
    size_t loop = 0;
    do {
        // bytes remaining in input and output
        size_t srcSize = in.size() - consumedSrc;
        size_t dstSize = out.size() - consumedDst;

        err = LZ4F_decompress(this->dctx, out.data() + consumedDst, &dstSize, 
                in.data() + consumedSrc, &srcSize, nullptr);
        if(LZ4F_isError(err)) {
            throw std::runtime_error(f("LZ4F_decompress() failed: {} ({:x})", 
                        LZ4F_getErrorName(err), err));
        }

        consumedSrc += srcSize;
        consumedDst += dstSize;
    } while(err != 0);

    // if we get here, we ostensibly finished decompressing
    return true;
}

/**
 * Decompression into a fixed size buffer.
 *
 * @return Total number of decompressed bytes.
 *
 * @TODO: This should probably be refactored so we can share decompress code with the vector one
 */
size_t LZ4::decompress(const std::vector<char> &in, void *out, const size_t outLen) {
   size_t err, consumedSrc = 0, consumedDst = 0;
    LZ4F_frameInfo_t info;
    memset(&info, 0, sizeof(info));

    char *outPtr = reinterpret_cast<char *>(out);

    // bail if input is empty
    if(in.empty()) {
        return 0;
    }

    // reset the context
    LZ4F_resetDecompressionContext(this->dctx);

    // try to read the frame parameters (get uncompressed size)
    if(in.size() < LZ4F_HEADER_SIZE_MAX) {
        throw std::runtime_error("Insufficient space for LZ4 header");
    }

    consumedSrc = in.size();
    err = LZ4F_getFrameInfo(this->dctx, &info, in.data(), &consumedSrc);
    if(LZ4F_isError(err)) {
        throw std::runtime_error(f("LZ4F_getFrameInfo() failed: {} ({:x})", 
                    LZ4F_getErrorName(err), err));
    }
    if(info.contentSize > outLen) {
        throw std::runtime_error("Insufficient buffer space");
    }

    // decode until done
    size_t loop = 0;
    do {
        // bytes remaining in input and output
        size_t srcSize = in.size() - consumedSrc;
        size_t dstSize = outLen - consumedDst;

        err = LZ4F_decompress(this->dctx, outPtr + consumedDst, &dstSize, 
                in.data() + consumedSrc, &srcSize, nullptr);
        if(LZ4F_isError(err)) {
            throw std::runtime_error(f("LZ4F_decompress() failed: {} ({:x})", 
                        LZ4F_getErrorName(err), err));
        }

        consumedSrc += srcSize;
        consumedDst += dstSize;
    } while(err != 0);

    return consumedDst;
}

