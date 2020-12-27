#include "TextureLoader.h"

#include "io/ResourceManager.h"
#include "io/Format.h"
#include <Logging.h>

#include <SOIL/SOIL.h>

#include <glm/glm.hpp>

#include <stdexcept>
#include <algorithm>
#include <cstddef>

using namespace world;

/**
 * Loads the given image from the textures resource bundle. It is decoded to 8-bit RGBA, which is
 * then converted to floating point and sqongled into the output buffer.
 *
 * This assumes that the input data is sRGB, and is converted to linear when loaded, if requested.
 */
void TextureLoader::load(const std::string &path, std::vector<float> &out, const bool sRgbConvert) {
    // read image data
    std::vector<unsigned char> data;
    io::ResourceManager::get(path, data);

    // load image
    int width = 0, height = 0;
    int channels;
    unsigned char *image = SOIL_load_image_from_memory(data.data(), data.size(), &width, &height,
            &channels, SOIL_LOAD_RGBA);
    if(!image) {
        const auto why = SOIL_last_result();
        Logging::error("Failed to load texture '{}': {}", path, why);
        throw std::runtime_error(f("Failed to load texture: {}", why));
    }

    // ensure we got an image that fits
    XASSERT((width * height * 4) <= out.size(), "Loaded texture too big ({} x {})", width, height);

    for(size_t y = 0; y < height; y++) {
        const size_t yOff = (y * width * 4);

        for(size_t x = 0; x < width; x++) {
            const size_t xOff = (x * 4);

            for(size_t c = 0; c < 4; c++) {
                const auto temp = image[yOff + xOff + c];
                out[yOff + xOff + c] = ((float) temp) / 255.;
            }
            // gamma correct RGB
            if(sRgbConvert) {
                const float gamma = 2.2;
                glm::vec3 color(out[yOff + xOff], out[yOff + xOff + 1], out[yOff + xOff + 2]);
                color = glm::pow(color, glm::vec3(gamma));
                out[yOff + xOff + 0] = color.x;
                out[yOff + xOff + 1] = color.y;
                out[yOff + xOff + 2] = color.z;
            }
        }
    }

    // free buffer
    SOIL_free_image_data(image);
}

