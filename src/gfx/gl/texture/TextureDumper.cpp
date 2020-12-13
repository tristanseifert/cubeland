/*
 * TextureDumper.cpp
 *
 *  Created on: Oct 23, 2015
 *      Author: tristan
 */

#include "TextureDumper.h"

#include <Logging.h>

using namespace std;
using namespace gfx;

static TextureDumper *dumper = new TextureDumper("texture_dump/");

/**
 * Initialises the texture dumper, storing the dumped texture data in the
 * specified folder.
 */
TextureDumper::TextureDumper(const string base) {
	this->outputFolder = base;
}

TextureDumper::~TextureDumper() {

}

/**
 * Gets the shared texture dumper object.
 */
TextureDumper *TextureDumper::sharedDumper() {
	return dumper;
}

/**
 * Adds the specified texture to the texture array.
 */
void TextureDumper::registerTexture(Texture *tex) {
	this->textures.push_back(tex);
}

/**
 * Removes the texture.
 */
void TextureDumper::removeTexture(Texture *tex) {
	this->textures.remove(tex);
}

/**
 * Dumps all textures to the base path.
 */
void TextureDumper::dump(void) {
	for(auto texture : this->textures) {
		texture->dump(this->outputFolder);
	}

    Logging::info("Dumped textures to {}", this->outputFolder);
}
