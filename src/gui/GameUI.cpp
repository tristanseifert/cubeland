#include "GameUI.h"
#include "PreferencesWindow.h"

#include <Logging.h>

#include <cmrc/cmrc.hpp>

#include <imgui.h>
#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_opengl3.h>

#include <cstring>

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
    {
        .path = "fonts/SourceSansPro-Italic.ttf",
        .name = "Source Sans Pro (Italic)",
    },
    {
        .path = "fonts/SourceSansPro-Bold.ttf",
        .name = "Source Sans Pro (Bold)",
    },
    {
        .path = "fonts/SourceSansPro-BoldItalic.ttf",
        .name = "Source Sans Pro (Bold + Italic)",
    },

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
};

const std::string GameUI::kRegularFontName = "Source Sans Pro (Regular)";
const std::string GameUI::kBoldFontName = "Source Sans Pro (Bold)";
const std::string GameUI::kItalicFontName = "Source Sans Pro (Italic)";
const std::string GameUI::kMonospacedFontName = "Space Mono (Regular)";

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

    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // set up the styles based on the window's scale factor
    // double scale = 2.0;
    double scale = 1.0;
    this->loadFonts(scale);

    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(1.0 / scale);

    // initialize the GL drawing layer
    ImGui_ImplSDL2_InitForOpenGL(this->window, context);
    ImGui_ImplOpenGL3_Init(nullptr);
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

    // then, load fonts
    for(const auto &info : kDefaultFonts) {
        file = fs.open(info.path);

        strncpy(cfg.Name, info.name.c_str(), sizeof(cfg.Name));
        font = io.Fonts->AddFontFromMemoryTTF((void *) file.begin(), (file.end() - file.begin()),
                floor(info.size * scale), &cfg);

        this->fonts[info.name] = font;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases all resources.
 */
GameUI::~GameUI() {
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
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(this->window);
    ImGui::NewFrame();

    // draw windows
    for(auto &w : this->windows) {
        if(w->isVisible()) {
            w->draw(this);
        }
    }
}

/**
 * Draws the UI on the current context.
 */
void GameUI::draw() {
    using namespace gl;
    const auto io = ImGui::GetIO();

    // perform the gui housekeeping for the frame
    ImGui::Render();

    // then, actually draw it. be sure alpha blending is enabled
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, 0, (int) io.DisplaySize.x, (int) io.DisplaySize.y);

    // glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    // glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
