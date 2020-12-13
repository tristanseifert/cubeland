#include "SceneRenderer.h"
#include "Drawable.h"
#include "WorldChunk.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/model/RenderProgram.h"
#include "gfx/model/Model.h"

#include <Logging.h>

#include <glbinding/gl/gl.h>

#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace render;

gl::GLuint VBO, VAO;

// World space positions of the objects
static const glm::vec3 cubePositions[] = {
    glm::vec3( 1.3f, -2.0f, -2.5f),
    glm::vec3( 0.0f,  0.0f,  0.0f),
    glm::vec3( 2.0f,  5.0f, -15.0f),
    glm::vec3(-1.5f, -2.2f, -2.5f),
    glm::vec3(-3.8f, -2.0f, -12.3f),
    glm::vec3( 2.4f, -0.4f, -3.5f),
    glm::vec3(-1.7f,  3.0f, -7.5f),
    glm::vec3( 1.5f,  2.0f, -2.5f),
    glm::vec3( 1.5f,  0.2f, -1.5f),
    glm::vec3(-1.3f,  1.0f, -1.5f)
};

/**
 * Init; this loads the program/shader we use normally for drawing
 */
SceneRenderer::SceneRenderer() {
    // Create a shader
    this->program = std::make_shared<gfx::RenderProgram>("/model/model.vert", "/model/model.frag", true);
    this->program->link();

    // load the model
    // this->model = std::make_shared<gfx::Model>("/teapot/teapot.obj");
    this->model = std::make_shared<WorldChunk>();
}

/**
 * Set up for rendering.
 */
void SceneRenderer::preRender(WorldRenderer *) {
    using namespace gl;

    // set clear colour and depth testing
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // set up culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

/**
 * Actually renders the scene.
 */
void SceneRenderer::render(WorldRenderer *renderer) {
    glm::mat4 projView = this->projectionMatrix * this->viewMatrix;
    this->_doRender(projView, this->program);
}

/**
 * Performs the actual rendering of the scene.
 */
void SceneRenderer::_doRender(glm::mat4 projView, std::shared_ptr<gfx::RenderProgram> program, bool hasNormalMatrix) {
    using namespace gl;

    program->bind();
    program->setUniformMatrix("projectionView", projView);

    // render
    for (int i = 0; i < 10; i++)  {
        // Calculate the model matrix for each object and pass it to shader before drawing
        glm::mat4 model;
        model = glm::translate(model, cubePositions[i]);

        GLfloat angle = 20.0f * i;

        float fasterTime = this->time / 5.f;
        //fasterTime *= (std::sqrt(this->time * 60.f) / 10.f) + 1;

        if(i % 3 == 0) {
            angle = fasterTime * -25.0f;
        } else if(i % 3 == 1) {
            angle = fasterTime * -15.0f;
        } else if(i % 3 == 2) {
            angle = fasterTime * -05.0f;
        }

//		model = glm::rotate(model, angle, glm::vec3(1.0f, 0.3f, 0.5f));
//        model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));
        program->setUniformMatrix("model", model);

        if(hasNormalMatrix == true) {
            glm::mat3 normalMatrix;
            normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
            program->setUniformMatrix("normalMatrix", normalMatrix);
        }

        this->model->draw(program);
    }

    gfx::VertexArray::unbind();
}

/**
 * Cleans up some state after rendering.
 */
void SceneRenderer::postRender(WorldRenderer *) {
    using namespace gl;

    // disable culling again
    glDisable(GL_CULL_FACE);

    // temp time advancing (for the animation)
    this->time += (1.0 / 60.0);
}
