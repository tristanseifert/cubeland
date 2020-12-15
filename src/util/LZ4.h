/**
 * Provides a basic LZ4 compress/decompress interface. We use the framed LZ4 compression format so
 * that we don't have to concern ourselves with storing sizes and other such info.
 *
 * LZ4 compression is used primarily for storing block data inside the world files.
 */
#ifndef UTIL_LZ4_H
#define UTIL_LZ4_H

#include <vector>

struct LZ4F_dctx_s;

namespace util {

/**
 * LZ4 compression machine
 *
 * This class is not thread safe; you may not use it from multiple threads simultaneously. The LZ4
 * context is shared between all invocations to a particular instance.
 */
class LZ4 {
    public:
        LZ4();
        virtual ~LZ4();

        void compress(const std::vector<char> &in, std::vector<char> &out);
        bool decompress(const std::vector<char> &in, std::vector<char> &out);

    private:
        // decompression context
        struct LZ4F_dctx_s *dctx = nullptr;
};

}

#endif
