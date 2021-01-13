#include "MainWindow.h"
#include "GameUI.h"
#include "MenuBarHandler.h"
#include "title/TitleScreen.h"
#include "render/WorldRenderer.h"

#include "util/CPUID.h"
#include "io/Format.h"
#include "io/MetricsManager.h"
#include "io/PrefsManager.h"

#include <Logging.h>

#include <mutils/time/profiler.h>
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>
#include <SDL.h>

#include <iterator>
#include <chrono>
#include <string>
#include <sstream>
#include <list>
#include <unordered_set>

using namespace gl;
using namespace gui;

/**
 * List of required OpenGL extensions
 */
static const std::list<std::string> kRequiredExtensions = {
    // occlusion queries and conditional rendering are used to cull chunks
    "GL_ARB_occlusion_query2",
    // "GL_NV_conditional_render" // part of core since OpenGL 3.0
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initializes the window.
 */
MainWindow::MainWindow() {
    int w = 0, h = 0;

    // check CPU extensions
    if(!util::CPUID::isAvxSupported()) {
        Logging::error("CPU is missing the AVX instruction set. Cannot continue");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CPU Requirements Error", "Your processor must support at least the AVX instruction set. This means at least AMD Jaguar/Bulldozer or Intel Sandy Bridge.", nullptr);
        exit(-1);
    }

    // set up GL and main window
    this->configGLContext();
    this->makeWindow();

    // set up profiling
    MUtils::Profiler::Init();
    MUtils::Profiler::NameThread("Main");

    // create the renderers
    auto bar = std::make_shared<MenuBarHandler>();
    this->gameUi = std::make_shared<GameUI>(this->win, this->winCtx);

    auto title = std::make_shared<TitleScreen>(this, this->gameUi);
    this->stages.push_back(title);

    this->stages.push_back(bar);
    this->stages.push_back(this->gameUi);

    // initialize renderers with current viewport size
    SDL_GL_GetDrawableSize(this->win, &w, &h);
    glViewport(0, 0, (GLsizei) w, (GLsizei) h);

    for(auto &render : this->stages) {
        render->reshape(w, h);
    }

    this->running = true;
}

/**
 * Sets up GL helper libraries.
 *
 * This is performed immediately after the first GL context is created.
 */
typedef void(* GlProc) (void);
static GlProc getProcAddr(const char *proc) {
    return (GlProc) SDL_GL_GetProcAddress(proc);
}

void MainWindow::initGLLibs() {
    using namespace glbinding;

    // set the address resolution callback
    initialize(getProcAddr);

    // call glGetError() after every call
    setCallbackMaskExcept(CallbackMask::After, { "glGetError" });
    setAfterCallback([](const FunctionCall &) {
        const auto error = glGetError();
        XASSERT(error == GL_NO_ERROR, "GL error: {}", error);
    });

    // check available extensions
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);

    std::unordered_set<std::string> extensions;
    extensions.reserve(numExtensions);

    for(size_t i = 0; i < numExtensions; i++) {
        const auto name = glGetStringi(GL_EXTENSIONS, i);
        extensions.emplace((char *) name);
    }

    for(const auto &name : kRequiredExtensions) {
        if(!extensions.contains(name)) {
            std::stringstream s;
            std::copy(std::begin(extensions), std::end(extensions), 
                    std::ostream_iterator<std::string>(s, ","));

            Logging::error("Missing required extension {}; available: {}", name, s.str());

            const auto body = f("A required OpenGL extension ({}) is missing. Update your graphics drivers and ensure they support at least OpenGL 4.1.", name);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenGL Error", body.c_str(), nullptr);

            exit(-1);
        }
    }
}

/**
 * Configures the SDL options for the context.
 *
 * We ask for at least a OpenGL 4.0 core context. Double buffering is requested.
 */
void MainWindow::configGLContext() {
    // opengl forawrd debug context flags
    const auto ctxFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG | 
                          (this->glDebug ? SDL_GL_CONTEXT_DEBUG_FLAG : 0);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, ctxFlags);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0); // no depth required
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1); // require hardware acceleration
}

/**
 * Creates the SDL window and its OpenGL context.
 */
void MainWindow::makeWindow() {
    int err;

    // create window; allowing for HiDPI contexts (if requested)
    const bool hiDpi = io::PrefsManager::getBool("window.hiDpi", true);

    const auto flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE |
                        (hiDpi ? SDL_WINDOW_ALLOW_HIGHDPI : 0);
    this->win = SDL_CreateWindow("Cubeland", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            kDefaultWidth, kDefaultHeight, flags);

    XASSERT(this->win != nullptr, "Failed to create window: {}", SDL_GetError());

    // resize window to stored size if needed
    if(io::PrefsManager::getBool("window.restoreSize")) {
        this->restoreWindowSize();
    }

    // create the GL context and initialize our GL libs
    this->winCtx = SDL_GL_CreateContext(this->win);
    XASSERT(this->winCtx != nullptr, "Failed to create OpenGL context: {}", SDL_GetError());

    SDL_GL_MakeCurrent(this->win, this->winCtx);

    this->initGLLibs();

    // enable VSync if possible
    err = SDL_GL_SetSwapInterval(1);
    if(err) {
        Logging::error("Failed to enable vsync ({}): {}", err, SDL_GetError());
    }

    // set some context defaults
    glClearColor(0, 0, 0, 1);

    // log info on the GL version
    Logging::info("GL version {}; vendor {}, renderer {}", glGetString(GL_VERSION),
                  glGetString(GL_VENDOR), glGetString(GL_RENDERER));

    // all subsequent context will share objects with this buffer
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases all SDL resources.
 */
MainWindow::~MainWindow() {
    // get rid of renderers
    this->stages.clear();

    // destroy window and context
    if(this->winCtx) {
        SDL_GL_DeleteContext(this->winCtx);
        this->winCtx = nullptr;
    }
    if(this->win) {
        SDL_DestroyWindow(this->win);
        this->win = nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Makes the window visible.
 */
void MainWindow::show() {
    XASSERT(this->win, "Window must exist");

    // capture mouse
    // this->setMouseCaptureState(true);

    /**
     * MacOS kludge: we need to give SDL an extra hint to make relative mouse
     * mode work. (see https://forums.libsdl.org/viewtopic.php?p=50057)
     */
#if MACOS
    Logging::info("Using relative mouse mode warp kludge");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
#endif

    SDL_ShowWindow(this->win);
}

/**
 * Sets the mouse capture state.
 */
void MainWindow::setMouseCaptureState(bool captured) {
    if(captured) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_ShowCursor(1);
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(0);
    }
}

/**
 * Runs the window event loop.
 *
 * @return Reason the window closed.
 */
int MainWindow::run() {
    int w, h;
    int reason = 0;
    SDL_Event event;

    Logging::trace("Entering main loop");

    // main run loop
    while(this->running) {
        // start the FPS counting
        MUtils::Profiler::NewFrame();
        auto frameStart = std::chrono::high_resolution_clock::now();

        this->startFrameFpsUpdate();

        // handle events
        {
            PROFILE_SCOPE(HandleEvents);
            while (SDL_PollEvent(&event)) {
                this->handleEvent(event, reason);
            }
        }

        // prepare renderers
        this->updateStages();

        {
            PROFILE_SCOPE(WillBeginFrame);
            for(auto rit = this->stages.rbegin(); rit != this->stages.rend(); ++rit) {
                auto &render = *rit;
                render->willBeginFrame();
            }
        }

        // render the profiler UI
        if(this->showProfiler) {
            PROFILE_SCOPE(ProfilerUI);
            MUtils::Profiler::ShowProfile(&this->showProfiler);
        }

        // clear the output buffer, then draw the scene and UI ontop
        {
            PROFILE_SCOPE(Draw);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            SDL_GL_GetDrawableSize(this->win, &w, &h);
            glViewport(0, 0, (GLsizei) w, (GLsizei) h);

            for(auto &render : this->stages) {
                render->draw();
            }
        }

        // swap buffers (this will synchronize to vblank if enabled)
        {
            PROFILE_SCOPE(WillEndFrame);
            for(auto &render : this->stages) {
                render->willEndFrame();
            }
        }

        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto diffUs = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart).count();
        io::MetricsManager::submitFrameTime(diffUs / 1000. / 1000.);

        this->endFrameFpsUpdate();
        {
            PROFILE_SCOPE(SwapWindow);
            SDL_GL_SwapWindow(this->win);
        }

        // invoke the final frame lifecycle callback and set up for the next one
        PROFILE_SCOPE(DidEndFrame);
        for(auto &render : this->stages) {
            render->didEndFrame();
        }
    }

    return reason;
}

/**
 * Handles events provided by SDL.
 */
void MainWindow::handleEvent(const SDL_Event &event, int &reason) {
    int w, h;

    // always handle some events
    switch(event.type) {
        // window events
        case SDL_WINDOWEVENT: {
            // ignore events for other windows
            auto ourId = SDL_GetWindowID(this->win);
            if(event.window.windowID != ourId) {
                break;
            }

            switch (event.window.event) {
                // window resized; reshape renderers
                case SDL_WINDOWEVENT_RESIZED: {
                    PROFILE_SCOPE(Reshape);

                    // update viewport
                    SDL_GL_GetDrawableSize(this->win, &w, &h);
                    glViewport(0, 0, (GLsizei) w, (GLsizei) h);

                    this->saveWindowSize();

                    for(auto &render : this->stages) {
                        render->reshape(w, h);
                    }
                    break;
                }

                // we ignore all other window events
                default:
                    break;
            }
            break;
        }

        // certain key events
        case SDL_KEYDOWN: {
            const auto &k = event.key.keysym;

            // toggle the profiler display
            if(k.scancode == SDL_SCANCODE_F7) {
                this->showProfiler = !this->showProfiler;
            }

            // TODO: ensure cursor is visible if not already
            if(this->showProfiler) {

            } else {

            }
            break;
        }

        // quit
        case SDL_QUIT:
            this->running = false;
            reason = 1;
            break;
    }

    // provide events to the UI stages in reverse order
    PROFILE_SCOPE(StageEvent);
    for(auto rit = this->stages.rbegin(); rit != this->stages.rend(); ++rit) {
        auto &render = *rit;

        if(render->handleEvent(event)) {
            // all handlers receive ESC key down
            if(event.type != SDL_KEYDOWN && event.key.keysym.scancode != SDL_SCANCODE_ESCAPE) {
                return;
            }
        }
    }
}

/**
 * Restores the window size.
 */
void MainWindow::restoreWindowSize() {
    int w = io::PrefsManager::getUnsigned("window.width", kDefaultWidth);
    int h = io::PrefsManager::getUnsigned("window.height", kDefaultHeight);
    XASSERT(w > 0 && h > 0, "Invalid window size (w {}, h {})", w, h);

    SDL_SetWindowSize(this->win, w, h);
    SDL_SetWindowPosition(this->win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}
/**
 * Gets the size of the window and writes it to the preferences.
 */
void MainWindow::saveWindowSize() {
    int w = 0, h = 0;

    SDL_GetWindowSize(this->win, &w, &h);
    XASSERT(w > 0 && h > 0, "Invalid window size (w {}, h {})", w, h);

    io::PrefsManager::setUnsigned("window.width", w);
    io::PrefsManager::setUnsigned("window.height", h);
}



/**
 * Start of frame handler for fps counting
 */
void MainWindow::startFrameFpsUpdate() {
    this->frameStartTime = (unsigned long) SDL_GetPerformanceCounter();
}

/**
 * Calculates the length of the frame, and if enough samples have been acquired, calculates the
 * average time needed to render a frame.
 */
void MainWindow::endFrameFpsUpdate() {
    // Calculate the difference in milliseconds
    unsigned long now = (unsigned long) SDL_GetPerformanceCounter();
    unsigned long difference = now - this->frameStartTime;

    double frequency = (double) SDL_GetPerformanceFrequency();
    this->frameTimeLast = (((double) difference) / frequency) * 1000.f;

    // Push it into the averaging queue.
    this->frameTimes.push(this->frameTimeLast);

    // If we have enough entries, average.
    if(this->frameTimes.size() == kNumFrameValues) {
        // calculate an average
        this->frameTimeAvg = 0.f;

        for(size_t i = 0; i < kNumFrameValues; i++) {
            this->frameTimeAvg += this->frameTimes.front();
            this->frameTimes.pop();
        }

        this->frameTimeAvg /= (double) kNumFrameValues;
    }

    // increment frame counter
    this->framesExecuted++;
}

/**
 * At the next main loop iteration, ensures the main window is closed.
 */
void MainWindow::quit() {
    Logging::debug("MainWindow::quit() called");
    this->running = false;
}

/**
 * Processes changes to the steps list.
 */
void MainWindow::updateStages() {
    int w, h;

    while(!this->stageChanges.empty()) {
        const auto req = this->stageChanges.front();
        this->stageChanges.pop();

        switch(req.type) {
            case StageChanges::SET_PRIMARY: {
                // ensure it's the correct size
                SDL_GL_GetDrawableSize(this->win, &w, &h);
                req.step->reshape(w, h);

                this->stages[0] = req.step;
                break;
            }

            default:
                XASSERT(false, "Unsupported change type: {}", req.type);
        }
    }
}

/**
 * Sets the primary (0th) run loop step
 */
void MainWindow::setPrimaryStep(std::shared_ptr<RunLoopStep> step) {
    StageChanges change(step);
    change.type = StageChanges::SET_PRIMARY;

    this->stageChanges.push(change);
    this->setMouseCaptureState(true);
}
