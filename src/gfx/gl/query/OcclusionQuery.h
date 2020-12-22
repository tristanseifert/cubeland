#ifndef GFX_GL_QUERY_OCCLUSIONQUERY_H
#define GFX_GL_QUERY_OCCLUSIONQUERY_H

#include <glbinding/gl/types.h>

namespace gfx {
/**
 * Occlusion queries can be used to determine whether a given set of render commands produce any
 * sort of output against the framebuffer.
 *
 * Combined with conditional rendering, occlusion queries can be used.
 *
 * This implements support for the GL_ANY_SAMPLES_PASSED boolean-style occlusion queries. We don't
 * really care about the precise number of samples.
 */
class OcclusionQuery {
    public:
        OcclusionQuery();
        ~OcclusionQuery();

        void begin(bool conservative = false);
        void stop();

        bool isResultAvailable() const;
        bool didSamplesPass() const;

        void beginConditionalRender(bool wait = true, bool clipToQuery = false);
        void endConditionalRender();

    private:
        gl::GLenum target;
        gl::GLuint queryId = 0;
};
}

#endif
