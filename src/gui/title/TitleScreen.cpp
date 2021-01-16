#include "TitleScreen.h"
#include "AboutWindow.h"
#include "WorldSelector.h"
#include "PlasmaRenderer.h"
#include "gui/GameUI.h"
#include "gui/MainWindow.h"
#include "gui/PreferencesWindow.h"

#include "render/WorldRenderer.h"

#include "gfx/gl/buffer/Buffer.h"
#include "gfx/gl/buffer/VertexArray.h"
#include "gfx/gl/program/ShaderProgram.h"
#include "gfx/gl/texture/Texture2D.h"

#include <Logging.h>
#include <imgui.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
#include <SDL.h>

using namespace gui;

static const gl::GLfloat kQuadVertices[] = {
    -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
};

/**
 * Sets up the title screen.
 */
TitleScreen::TitleScreen(MainWindow *_win, std::shared_ptr<GameUI> &_gui) : win(_win), gui(_gui) {
    using namespace gfx;

    // TODO: handle this better!
    this->plasma = new title::PlasmaRenderer(glm::ivec2(1024, 768), 2);

    // load shader
    this->program = new ShaderProgram("title/background.vert", "title/background.frag");
    this->program->link();

    // allocate the vertex buffer for the quad
    this->vao = new VertexArray;
    this->vertices = new Buffer(Buffer::Array, Buffer::StaticDraw);

    this->vao->bind();
    this->vertices->bind();

    this->vertices->bufferData(sizeof(kQuadVertices), (void *) &kQuadVertices);

    this->vao->registerVertexAttribPointer(0, 3, VertexArray::Float, 5 * sizeof(gl::GLfloat), 0);
    this->vao->registerVertexAttribPointer(1, 2, VertexArray::Float, 5 * sizeof(gl::GLfloat),
            3 * sizeof(gl::GLfloat));

    VertexArray::unbind();

    // create background texture
    this->bgTexture = new gfx::Texture2D(3);
    this->bgTexture->setUsesLinearFiltering(true);
    this->bgTexture->setDebugName("TitleBackground");

    // set up the button window
    this->butts = std::make_shared<ButtonWindow>(this);
    _gui->addWindow(this->butts);
}

/**
 * Deletes all title screen resources.
 */
TitleScreen::~TitleScreen() {
    this->gui->removeWindow(this->butts);
    if(this->worldSel) {
        this->gui->removeWindow(this->worldSel);
    }
    if(this->prefs) {
        this->gui->removeWindow(this->prefs);
    } 
    if(this->about) {
        this->gui->removeWindow(this->about);
    }

    delete this->bgTexture;
    delete this->program;
    delete this->vao;
    delete this->vertices;

    delete this->plasma;
}



/**
 * When the step is about to render for the first time, enable cursor.
 */
void TitleScreen::stepAdded() {
    // force cursor to be displayed
    this->win->setMouseCaptureState(false);
}

/**
 * Updates the background drawing and timing for animations.
 */
void TitleScreen::willBeginFrame() {
    using namespace gl;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    this->plasma->draw(this->time / 5.);
    this->time += 1. / 60.;

    if(this->worldSel) {
        this->worldSel->startOfFrame();
    }
}

/**
 * Draws the main buttons for the title screen
 */
void TitleScreen::drawButtons(GameUI *gui) {
    // begin the window
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
    ImVec2 windowPos = ImVec2(io.DisplaySize.x / 2., io.DisplaySize.y / 2.);

    ImGui::SetNextWindowSize(ImVec2(400, 0));
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(.5, .5));

    if(!ImGui::Begin("Title Screen Buttons", nullptr, windowFlags)) {
        return;
    }

    // set big ass button font
    ImGui::PushFont(gui->getFont(GameUI::kGameFontHeading));

    // draw buttons
    if(ImGui::Button("Single Player", ImVec2(400, 0))) {
        if(!this->worldSel) {
            this->worldSel = std::make_shared<title::WorldSelector>(this);
            gui->addWindow(this->worldSel);
        }

        this->worldSel->loadRecents();
        this->worldSel->setVisible(true);
    }
    ImGui::Dummy(ImVec2(0,20));
    if(ImGui::Button("Preferences", ImVec2(400, 0))) {
        if(!this->prefs) {
            this->prefs = std::make_shared<PreferencesWindow>();
            gui->addWindow(this->prefs);
        }

        this->prefs->load();
        this->prefs->setVisible(true);
    }

    ImGui::PopFont();
    ImGui::PushFont(this->gui->getFont(GameUI::kGameFontHeading3));

    ImGui::Dummy(ImVec2(0,20));
    if(ImGui::Button("About", ImVec2(190, 0))) {
        if(!this->about) {
            this->about = std::make_shared<title::AboutWindow>();
            gui->addWindow(this->about);
        }

        this->about->setVisible(true);
    }
    ImGui::SameLine();
    if(ImGui::Button("Quit", ImVec2(190, 0))) {
        this->win->quit();
    }

    ImGui::PopFont();

    // finish
    ImGui::End();
}



/**
 * Draws the title screen.
 */
void TitleScreen::draw() {
    using namespace gl;
    int w, h;

    // update viewport
    auto win = this->win->getSDLWindow();
    SDL_GL_GetDrawableSize(win, &w, &h);
    glViewport(0, 0, (GLsizei) w, (GLsizei) h);

    // draw the plasma background layer
    auto plasmaTex = this->plasma->getOutput();
    plasmaTex->bind();

    this->program->bind();
    this->program->setUniform1i("texPlasma", plasmaTex->unit);

    if(this->showBackground) {
        this->bgTexture->bind();
        this->program->setUniform1i("texOverlay", this->bgTexture->unit);
        this->program->setUniform1f("overlayFactor", this->bgFactor);
    } else {
        this->program->setUniform1f("overlayFactor", 0);
    }

    this->program->setUniformVec("vignetteParams", this->vignetteParams);

    this->vao->bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    gfx::VertexArray::unbind();
}

/**
 * Resizes all our data/output textures as needed.
 */
void TitleScreen::reshape(unsigned int w, unsigned int h) {
    this->plasma->resize(glm::ivec2(w / kPlasmaScale, h / kPlasmaScale));
}



/**
 * Creates a world render, and replaces the title screen run loop step with it.
 */
void TitleScreen::openWorld(std::shared_ptr<world::WorldSource> &source) {
    // create renderer
    auto rend = std::make_shared<render::WorldRenderer>(this->win, this->gui, source);

    // insert it
    this->win->setPrimaryStep(rend);
}



/**
 * Sets the background image.
 */
void TitleScreen::setBackgroundImage(const std::vector<std::byte> &data, const glm::ivec2 &size,
        const bool animate) {
    // select correct texture
    gfx::Texture2D *tex = this->bgTexture;

    // transfer
    XASSERT(tex, "Invalid background texture");

    tex->allocateBlank(size.x, size.y, gfx::Texture2D::RGBA8);
    tex->bufferSubData(size.x, size.y, 0, 0,  gfx::Texture2D::RGBA8, data.data());

    // TODO: handle crossfading
    this->bgFactor = 1;
    this->showBackground = true;
}

/**
 * Clears the background image.
 */
void TitleScreen::clearBackgroundImage(const bool animate) {
    this->showBackground = false;
}
