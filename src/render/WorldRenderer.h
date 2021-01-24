#ifndef RENDER_WORLDRENDERER_H
#define RENDER_WORLDRENDERER_H

#include "gui/RunLoopStep.h"
#include "gui/GameWindow.h"
#include "world/ClientWorldSource.h"
#include "Camera.h"

#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <variant>
#include <vector>

#include <blockingconcurrentqueue.h>

namespace chat {
class Manager;
}

namespace gui {
class MainWindow;
class GameUI;
class InGamePrefsWindow;
}
namespace inventory {
class Manager;
class UI;
}

namespace input {
class InputManager;
class BlockInteractions;
class PlayerPosPersistence;
}

namespace world {
class ClientWorldSource;
class TimePersistence;
}

namespace physics {
class Engine;
}

namespace render {
class RenderStep;
class Lighting;
class HDR;
class FXAA;
class WorldRendererDebugger;

class WorldRenderer: public gui::RunLoopStep {
    friend class WorldRendererDebugger;

    public:
        WorldRenderer(gui::MainWindow *, std::shared_ptr<gui::GameUI> &, std::shared_ptr<world::ClientWorldSource> &);
        virtual ~WorldRenderer();

    public:
        void willBeginFrame() override;
        void draw() override;
        void willQuit() override;
        void willEndFrame() override;

        void reshape(unsigned int width, unsigned int height) override;
        bool handleEvent(const SDL_Event &) override;

        void requestPrefsLoad() {
            this->needsPrefsLoad = true;
        }

        /// returns a vector of (zNear, zFar) for clipping
        glm::vec2 getZPlane(void) const {
            return glm::vec2(this->zNear, this->zFar);
        }
        /// gets a const reference to the camera
        const Camera &getCamera() const {
            return this->camera;
        }
        /// gets the field of view, in degrees
        const float getFoV() const {
            return this->projFoV;
        }
        /// returns the viewport aspect ratio
        const float getAspect() const {
            return this->aspect;
        }
        /// returns the current world time
        const double getTime() const {
            return this->source->getTime();
        }

        void loadPrefs();

    private:
        /// game window for drawing pause menu
        class PauseWindow: public gui::GameWindow {
            public:
                PauseWindow(WorldRenderer *_rend) : renderer(_rend) {}
                virtual ~PauseWindow() = default;

                // forward draw calls into the title screen class
                void draw(gui::GameUI *gui) override {
                    this->renderer->drawPauseButtons(gui);
                }

            private:
                WorldRenderer *renderer = nullptr;
        };

        /// indicates a screenshot is to be saved
        struct SaveScreenshot {
            // take ownership and release this later
            std::byte *data;
            // size of screenshot image
            glm::ivec2 size;
        };

        using WorkItem = std::variant<std::monostate, SaveScreenshot>;

        /// JPEG quality factor for world preview images
        constexpr static const int kPreviewQuality = 74;

    private:
        void updateView();

        void drawPauseButtons(gui::GameUI *);
        void animatePauseMenu();
        void openPauseMenu();
        void closePauseMenu();
        void captureScreenshot();
        void saveScreenshot();

        void workerMain();
        void workerSaveScreenshot(const SaveScreenshot &);

    private:
        std::unique_ptr<std::thread> worker = nullptr;
        std::atomic_bool workerRun;
        moodycamel::BlockingConcurrentQueue<WorkItem> work;

        // used for keyboard/game controller input
        input::InputManager *input = nullptr;
        input::PlayerPosPersistence *posSaver = nullptr;
        // debugging
        WorldRendererDebugger *debugger = nullptr;

        input::BlockInteractions *blockInt = nullptr;

        inventory::Manager *inventory = nullptr;
        std::shared_ptr<inventory::UI> inventoryUi = nullptr;

        std::shared_ptr<gui::GameUI> gui;
        gui::MainWindow *window = nullptr;

        physics::Engine *physics = nullptr;

        world::TimePersistence *timeSaver = nullptr;

        // for drawing the pause buttons
        std::shared_ptr<PauseWindow> pauseWin = nullptr;
        // in game preferences
        std::shared_ptr<gui::InGamePrefsWindow> prefsWin = nullptr;

        // chat interface for multiplayer games
        chat::Manager *chat = nullptr;

    private:
        /// Duration in the pause fade-out, in seconds
        constexpr static const float kPauseAnimationDuration = 1.;

        /// set when we're going to quit soon
        bool isQuitting = false;

        // near and far clipping planes
        float zNear = 0.1f;
        float zFar = 1250.f;
        // field of view, in degrees
        float projFoV = 74.f;
        // size of the viewport (render canvas)
        unsigned int viewportWidth = 0, viewportHeight = 0;
        /// viewport aspect ratio
        float aspect = 0.f;

        // projection matrix
        glm::mat4 projection;

        // camera
        Camera camera;

        // render stages
        std::vector<std::shared_ptr<RenderStep>> steps;

        // world source providing all data for this world
        std::shared_ptr<world::ClientWorldSource> source;

        std::shared_ptr<Lighting> lighting = nullptr;
        std::shared_ptr<HDR> hdr = nullptr;
        std::shared_ptr<FXAA> fxaa = nullptr;

        // whether the debugger is visible
        bool isDebuggerOpen = false;
        uint32_t debugItemToken = 0;

        // whether the pause menu is being shown
        bool isPauseMenuOpen = false;
        // whether to fade out the game content
        bool isPauseMenuAnimating = false;
        // set to exit to title screen at the start of next frame
        size_t exitToTitle = 0;
        // set to quit instead of exit to title
        bool needsQuit = false;
        // time at which the menu was opened (for animation)
        std::chrono::steady_clock::time_point menuOpenedAt;
        // when set, we capture a screenshot of the world after rendering it next frame
        bool needsScreenshot = false;
        // screenshot buffer
        std::byte *screenshot = nullptr;
        // force prefs to load at the end of next frame
        bool needsPrefsLoad = false;
};
}

#endif
