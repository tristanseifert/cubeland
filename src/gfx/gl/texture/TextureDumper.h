/*
 * TextureDumper.h
 *
 * Allows textures to register themselves with the dumper, so that their 'dump'
 * routine is called when all others are being dumped as well.
 *
 *  Created on: Oct 23, 2015
 *      Author: tristan
 */

#ifndef GFX_BUFFER_TEXTURE_TEXTUREDUMPER_H_
#define GFX_BUFFER_TEXTURE_TEXTUREDUMPER_H_

#include <list>
#include <string>

#include <fstream>

#include "Texture.h"

namespace gfx {
class TextureDumper {
    public:
        TextureDumper(const std::string base);
        virtual ~TextureDumper();

        static TextureDumper *sharedDumper();

        void registerTexture(Texture *tex);
        void removeTexture(Texture *tex);

        void dump();

    private:
        std::list<Texture*> textures;

        std::string outputFolder;
};
} /* namespace gfx */

#endif /* GFX_BUFFER_TEXTURE_TEXTUREDUMPER_H_ */
