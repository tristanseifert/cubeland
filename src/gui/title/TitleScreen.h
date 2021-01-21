/**
 * Handles combining the OpenGL background and the GUI elements of the title screen.
 */
#ifndef GUI_TITLE_TITLESCREEN_H
#define GUI_TITLE_TITLESCREEN_H

#include "gui/RunLoopStep.h"
#include "gui/GameWindow.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>

namespace world {
class ClientWorldSource;
}

namespace gfx {
class Buffer;
class ShaderProgram;
class VertexArray;
class Texture2D;
}

namespace gui {
class MainWindow;
class GameUI;
class PreferencesWindow;

namespace title {
class AboutWindow;
class WorldSelector;
class ServerSelector;
class PlasmaRenderer;
}

class TitleScreen: public RunLoopStep {
    friend class title::WorldSelector;
    friend class title::ServerSelector;

    public:
        TitleScreen(MainWindow *, std::shared_ptr<GameUI> &);
        ~TitleScreen();

    public:
        void stepAdded() override;
        void willBeginFrame() override;
        void draw() override;

        void reshape(unsigned int w, unsigned int h) override;

        // we ignore SDL events. don't have any use for them currently
        bool handleEvent(const SDL_Event &) override {
            return false;
        }

    protected:
        void openWorld(std::shared_ptr<world::ClientWorldSource>);

        void setVignetteParams(const glm::vec2 &params) {
            this->setVignetteParams(params.x, params.y);
        }
        void setVignetteParams(const float radius, const float smoothness) {
            this->vignetteParams = glm::vec2(radius, smoothness);
        }

        const bool isBgVisible() const {
            return this->showBackground;
        }

        void clearBackgroundImage(const bool animate = true);
        void setBackgroundImage(const std::vector<std::byte> &data, const glm::ivec2 &size, const bool animate = true);

    private:
        // game window that just calls into our drawing routines
        class ButtonWindow: public gui::GameWindow {
            public:
                ButtonWindow(TitleScreen *_title) : title(_title) {}
                virtual ~ButtonWindow() = default;

                // forward draw calls into the title screen class
                void draw(GameUI *gui) override {
                    this->title->drawButtons(gui);
                }

            private:
                TitleScreen *title = nullptr;
        };

    private:
        void drawButtons(GameUI *gui);

        void animateBackground();

    private:
        /// divide the viewport size by this factor for the plasma effect
        constexpr static const float kPlasmaScale = 4.;

        /// duration to fade in a background image, in seconds
        constexpr static const float kBgFadeInDuration = .66;
        /// duration to fade out the background image, in seconds
        constexpr static const float kBgFadeOutDuration = 1.;
        /// duration of crossfading between images
        constexpr static const float kBgCrossfadeDuration = 1.33;

        /// vignetting factors for displaying backgrounds
        constexpr static const glm::vec2 kBgVignette = glm::vec2(.633, .5);

        enum class AnimationType {
            /// No in-progress animation
            None,
            /// No image is visible yet; fade in background 0
            FadeIn1,
            /// Whatever image is currently visible, fade it out.
            FadeOut,
            /// Image 1 is visible; crossfade to image 2
            Crossfade1To2,
            /// Image 2 is visible; crossfade to image 1
            Crossfade2To1,
        };

    private:
        MainWindow *win = nullptr;
        std::shared_ptr<GameUI> gui = nullptr;

        std::shared_ptr<ButtonWindow> butts = nullptr;

        std::shared_ptr<PreferencesWindow> prefs = nullptr;
        std::shared_ptr<title::AboutWindow> about = nullptr;
        std::shared_ptr<title::WorldSelector> worldSel = nullptr;
        std::shared_ptr<title::ServerSelector> serverSel = nullptr;

        title::PlasmaRenderer *plasma = nullptr;

        // used for timing of background animations
        double time = 0.;

        // background drawing shader (applies a small blur)
        gfx::ShaderProgram *program = nullptr;
        // buffer holding vertices for the full screen quad
        gfx::Buffer *vertices = nullptr;
        // vertex array defining vertices
        gfx::VertexArray *vao = nullptr;

        // whether background is shown
        bool showBackground = false;
        // background image texture
        gfx::Texture2D *bgTextures[2] = { nullptr, nullptr };
        // opacity of the background texture
        float bgFactor = 0;
        // mix facter between the two bg textures
        float bgMixFactor = 0;
        // vignetting parameters
        glm::vec2 vignetteParams = glm::vec2(1, 0);

        /// animation for background
        AnimationType bgAnim = AnimationType::None;
        /// time point to use for the start of the bg animation
        std::chrono::steady_clock::time_point bgAnimationStart;

        // used to calculate time difference between frames (for plasma effect)
        std::chrono::steady_clock::time_point lastFrame;
};
}

#endif
