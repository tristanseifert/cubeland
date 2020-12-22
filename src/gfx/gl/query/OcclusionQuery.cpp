#include "OcclusionQuery.h"

#include <glbinding/gl/gl.h>

using namespace gl;
using namespace gfx;

/**
 * Constructs the occlusion query.
 */
OcclusionQuery::OcclusionQuery() {
    glGenQueries(1, &this->queryId);
}

/**
 * Deletes the occlusion query object.
 */
OcclusionQuery::~OcclusionQuery() {
    glDeleteQueries(1, &this->queryId);
}


/**
 * Begins the occlusion query scope. All draw calls between this call, and a matching call to
 * `end()` will be used to determine whether any samples are written.
 */
void OcclusionQuery::begin(bool conservative) {
    this->target = conservative ? GL_ANY_SAMPLES_PASSED_CONSERVATIVE : GL_ANY_SAMPLES_PASSED;
    glBeginQuery(this->target, this->queryId);
}

/**
 * Ends the occlusion query.
 */
void OcclusionQuery::stop() {
    glEndQuery(this->target);
}

/**
 * Checks if the query results are available.
 */
bool OcclusionQuery::isResultAvailable() const {
    GLuint avail = (GLuint) GL_FALSE;
    glGetQueryObjectuiv(this->queryId, GL_QUERY_RESULT_AVAILABLE, &avail);
    return (avail == GL_TRUE);
}

/**
 * Retrieves the result of the occlusion query. If the query is not yet ready, this call will stall
 * the caller.
 */
bool OcclusionQuery::didSamplesPass() const {
    GLuint passed = (GLuint) GL_FALSE;
    glGetQueryObjectuiv(this->queryId, GL_QUERY_RESULT, &passed);
    return (passed == GL_TRUE);
}

/**
 * Begins a conditional rendering block, using the results of this query to determine whether the
 * calls between this call, and a matching `endConditionalRender()` call are performed.
 */
void OcclusionQuery::beginConditionalRender(bool wait, bool clipToQuery) {
    GLenum mode = GL_QUERY_WAIT;

    if(wait) {
        mode = clipToQuery ? GL_QUERY_BY_REGION_WAIT : GL_QUERY_WAIT;
    } else {
        mode = clipToQuery ? GL_QUERY_BY_REGION_NO_WAIT : GL_QUERY_NO_WAIT;
    }

    glBeginConditionalRender(this->queryId, mode);
}
/**
 * Ends a conditional render.
 *
 * Note that even though the render block may now be complete, the GPU may stall if it needs to
 * wait for the results of the query.
 */
void OcclusionQuery::endConditionalRender() {
    glEndConditionalRender();
}
