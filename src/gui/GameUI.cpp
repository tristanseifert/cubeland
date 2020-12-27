#include "GameUI.h"
#include "PreferencesWindow.h"

#include "MetricsDisplay.h"
#include "io/MetricsManager.h"

#include <Logging.h>

#include <cmrc/cmrc.hpp>

#include <imgui.h>
#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cstring>

#include <mutils/time/profiler.h>
#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
#include <SDL.h>

CMRC_DECLARE(ui);

using namespace gui;

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Fonts to load from the resource catalog on startup
 */
const std::vector<GameUI::FontInfo> GameUI::kDefaultFonts = {
    // this is the default font (including regular/bold, and italic for each)
    {
        .path = "fonts/SourceSansPro-Regular.ttf",
        .name = "Source Sans Pro (Regular)",
    },
/*    {
        .path = "fonts/SourceSansPro-Italic.ttf",
        .name = "Source Sans Pro (Italic)",
    },*/
    {
        .path = "fonts/SourceSansPro-Bold.ttf",
        .name = "Source Sans Pro (Bold)",
    },
/*    {
        .path = "fonts/SourceSansPro-BoldItalic.ttf",
        .name = "Source Sans Pro (Bold + Italic)",
    },*/

    // the black version of Source Sans Pro is used for headings; accordingly, make it bigger
    {
        .path = "fonts/SourceSansPro-Black.ttf",
        .name = "Source Sans Pro (Black)",
        .size = 35,
    },

    // monospaced font
    {
        .path = "fonts/SpaceMono-Regular.ttf",
        .name = "Space Mono (Regular)",
    },
    {
        .path = "fonts/SpaceMono-Bold.ttf",
        .name = "Space Mono (Bold)",
    },

    // used for game UI
    {
        .path = "fonts/Overpass-Regular.ttf",
        .name = "Overpass (Regular)",
        .scalesWithUi = true,
    },
    {
        .path = "fonts/Overpass-Bold.ttf",
        .name = "Overpass (Bold)",
        .scalesWithUi = true,
    }
};

const std::string GameUI::kRegularFontName = "Source Sans Pro (Regular)";
const std::string GameUI::kBoldFontName = "Source Sans Pro (Bold)";
const std::string GameUI::kItalicFontName = "Source Sans Pro (Italic)";

const std::string GameUI::kMonospacedFontName = "Space Mono (Regular)";
const std::string GameUI::kMonospacedBoldFontName = "Space Mono (Bold)";

const std::string GameUI::kGameFontRegular = "Overpass (Regular)";
const std::string GameUI::kGameFontBold = "Overpass (Bold)";

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Sets up the user interface layer.
 *
 * Initializes Dear IMGui, including the definition of styles and font loading.
 *
 * TODO: handle re-loading fonts if scale factor changes
 */
GameUI::GameUI(SDL_Window *_window, void *context) : window(_window) {
    // check version and initialize the context, styles
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    // io.MouseDrawCursor = true;

    // get window physical size
    int w, cw, h, ch;
    SDL_GetWindowSize(_window, &w, &h);
    SDL_GL_GetDrawableSize(_window, &cw, &ch);

    double xScale = ((double) cw) / ((double) w);
    double yScale = ((double) ch) / ((double) h);
    double scale = std::max(xScale, yScale);

    Logging::debug("Window size {}x{}, context size {}x{} -> scale {}", w, h, cw, ch, scale);

    // set up the styles based on the window's scale factor
    this->loadFonts(1 /*scale*/);

    ImGui::StyleColorsDark();
    // ImGui::GetStyle().ScaleAllSizes(1.0 / scale);
    // ImGui::GetStyle().ScaleAllSizes(1.0 / scale);

    io.DisplayFramebufferScale.x = xScale;
    io.DisplayFramebufferScale.y = yScale;

    // initialize the GL drawing layer
    ImGui_ImplSDL2_InitForOpenGL(this->window, context);
    ImGui_ImplOpenGL3_Init(nullptr);

    // create the metrics display
    auto md = std::make_shared<MetricsDisplay>();
    this->addWindow(md);

    io::MetricsManager::setDisplay(md);
}

/**
 * Adds a new window to the UI.
 */
void GameUI::addWindow(std::shared_ptr<GameWindow> window) {
    // inscrete
    this->windows.push_back(window);

    // sort by the "uses game styling" flag
    std::sort(std::begin(this->windows), std::end(this->windows), [](const auto &l, const auto &r){
        return (l->usesGameStyle() < r->usesGameStyle());
    });
}

/**
 * Removes a window from the game UI.
 */
void GameUI::removeWindow(std::shared_ptr<GameWindow> window) {
    this->windows.erase(std::remove(std::begin(this->windows), std::end(this->windows), window),
            std::end(this->windows));
}

/**
 * On display size update, propagate this to the UI layer.
 */
void GameUI::reshape(unsigned int width, unsigned int height) {
    // TODO: do we need to do anything?
}

/**
 * Pulls the font resources out and loads them into the UI layer.
 */
void GameUI::loadFonts(const double scale) {
    cmrc::file file;

    ImGuiIO& io = ImGui::GetIO();
    ImFont *font = nullptr;

    auto fs = cmrc::ui::get_filesystem();

    // configuration for the font
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;

    io.Fonts->Clear();

    // then, load fonts
    for(const auto &info : kDefaultFonts) {
        float fontScale = scale;
        if(info.scalesWithUi) {
            fontScale *= this->scale;
        }

        file = fs.open(info.path);

        strncpy(cfg.Name, info.name.c_str(), sizeof(cfg.Name));
        font = io.Fonts->AddFontFromMemoryTTF((void *) file.begin(), (file.end() - file.begin()),
                floor(info.size * fontScale), &cfg);

        this->fonts[info.name] = font;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases all resources.
 */
GameUI::~GameUI() {
    // ensure all other modules no longer hold refs to our GUI elements
    io::MetricsManager::setDisplay(nullptr);

    // shut down ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();

    ImGui::DestroyContext();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Forwards an event received from SDL to the GUI layer.
 *
 * @return Whether the event was consumed by the GUI layer.
 */
bool GameUI::handleEvent(const SDL_Event &event) {
    PROFILE_SCOPE(GuiEvents);

    const auto io = ImGui::GetIO();
    ImGui_ImplSDL2_ProcessEvent(&event);

    // if we desire keyboard events, and this is one of those, we swallow it
    if(event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_TEXTEDITING ||
       event.type == SDL_TEXTINPUT || event.type == SDL_KEYMAPCHANGED) {
        return io.WantCaptureKeyboard;
    }
    // same but for mouse events
    if(event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
       event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEWHEEL) {
        return io.WantCaptureMouse;
    }

    // otherwise, don't swallow the event
    return false;
}

/**
 * Prepares for the next frame of rendering.
 */
void GameUI::willBeginFrame() {
    PROFILE_SCOPE(GuiStartFrame);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(this->window);
    ImGui::NewFrame();

    // draw windows
    bool appliedStyle = false;

    for(auto &w : this->windows) {
        // ignore invisible windows
        if(!w->isVisible()) continue;

        // apply game style if needed
        if(!appliedStyle && w->usesGameStyle()) {
            this->pushGameStyles();
            appliedStyle = true;
        }

        // let the window draw
        w->draw(this);
    }

    // pop the game styles again
    if(appliedStyle) {
        this->popGameStyles();
    }
}

/**
 * Draws the UI on the current context.
 */
void GameUI::draw() {
    PROFILE_SCOPE(GuiDraw);

    using namespace gl;
    const auto io = ImGui::GetIO();

    // perform the gui housekeeping for the frame
    ImGui::Render();

    // then, actually draw it. be sure alpha blending is enabled
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

/**
 * Applies the game UI style.
 */
void GameUI::pushGameStyles() {
    // default font
    ImGui::PushFont(this->getFont(kGameFontRegular));

    // new colors
    ImGui::PushStyleColor(ImGuiCol_TitleBg, glm::vec4(0, 0, 0, 1));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, glm::vec4(.31, 0, 0, 1));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, glm::vec2(.5, .5));
}

/**
 * Removes all styles we added.
 */
void GameUI::popGameStyles() {
    ImGui::PopFont();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}
