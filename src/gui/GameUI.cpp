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
    },
    {
        .path = "fonts/Overpass-Bold.ttf",
        .name = "Overpass (Heading 1)",
        .scalesWithUi = true,
        .size = 35,
    },
    {
        .path = "fonts/Overpass-Bold.ttf",
        .name = "Overpass (Heading 2)",
        .scalesWithUi = true,
        .size = 24,
    },
    {
        .path = "fonts/Overpass-Bold.ttf",
        .name = "Overpass (Heading 3)",
        .scalesWithUi = true,
        .size = 18,
    },

    {
        .path = "fonts/SourceSansPro-Regular.ttf",
        .name = "Body (Regular)",
        .scalesWithUi = true,
    },
    {
        .path = "fonts/SourceSansPro-Italic.ttf",
        .name = "Body (Italic)",
        .scalesWithUi = true,
    },

    {
        .path = "fonts/SpaceMono-Regular.ttf",
        .name = "Monospaced (Regular)",
        .scalesWithUi = true,
    },
};

const std::string GameUI::kRegularFontName = "Source Sans Pro (Regular)";
const std::string GameUI::kBoldFontName = "Source Sans Pro (Bold)";
const std::string GameUI::kItalicFontName = "Source Sans Pro (Italic)";

const std::string GameUI::kMonospacedFontName = "Space Mono (Regular)";
const std::string GameUI::kMonospacedBoldFontName = "Space Mono (Bold)";

const std::string GameUI::kGameFontRegular = "Overpass (Regular)";
const std::string GameUI::kGameFontBold = "Overpass (Bold)";
const std::string GameUI::kGameFontHeading = "Overpass (Heading 1)";
const std::string GameUI::kGameFontHeading2 = "Overpass (Heading 2)";
const std::string GameUI::kGameFontHeading3 = "Overpass (Heading 3)";

const std::string GameUI::kGameFontBodyRegular = "Body (Regular)";
const std::string GameUI::kGameFontBodyItalic = "Body (Italic)";
const std::string GameUI::kGameFontBodyBold = "Body (Bold)";

const std::string GameUI::kGameFontMonospaced = "Monospaced (Regular)";

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
    UpdateRequest req(window);
    req.type = UpdateRequest::ADD;

    this->requests.push(req);
}

/**
 * Removes a window from the game UI.
 */
void GameUI::removeWindow(std::shared_ptr<GameWindow> window) {
    UpdateRequest req(window);
    req.type = UpdateRequest::REMOVE;

    this->requests.push(req);
}

void GameUI::removeWindow(GameWindow *window) {
    UpdateRequest req(window);
    req.type = UpdateRequest::REMOVE;

    this->requests.push(req);
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

    // process additions/removals
    bool sort = false;

    while(!this->requests.empty()) {
        // get frontmost request
        const auto req = this->requests.front();
        this->requests.pop();

        XASSERT(req.window || req.rawWindow, "Null window request");

        // handle it by type
        switch(req.type) {
            // insert a new window
            case UpdateRequest::ADD:
                this->windows.push_back(req.window);
                sort = true;
                break;

            // remove the window
            case UpdateRequest::REMOVE:
                if(req.window) {
                this->windows.erase(std::remove(std::begin(this->windows), std::end(this->windows),
                            req.window), std::end(this->windows));
                } else if(req.rawWindow) {
                    const auto rawWin = req.rawWindow;
                    this->windows.erase(std::remove_if(std::begin(this->windows), std::end(this->windows),
                        [&, rawWin](const auto &in) {
                        return (in.get() == rawWin);
                    }), std::end(this->windows));
                }
                break;
        }
    }

    if(sort) {
        // sort by the "uses game styling" flag
        std::sort(std::begin(this->windows), std::end(this->windows), [](const auto &l, const auto &r){
            return (l->usesGameStyle() < r->usesGameStyle());
        });
    }

    // draw windows
    bool appliedStyle = false;

    for(auto &w : this->windows) {
        // ignore invisible windows
        if(!w->isVisible() && w->skipDrawIfInvisible()) continue;

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



const static int Black                  = 0x00000000;
const static int White                  = 0xFFFFFF00;

const static int AlphaTransparent       = 0x00;
const static int Alpha20                = 0x33;
const static int Alpha40                = 0x66;
const static int Alpha50                = 0x80;
const static int Alpha60                = 0x99;
const static int Alpha80                = 0xCC;
const static int Alpha90                = 0xE6;
const static int AlphaFull              = 0xFF;

static float GetR(int colorCode) {
    return (float)((colorCode & 0xFF000000) >> 24 ) / (float)(0xFF);
}
static float GetG(int colorCode) {
    return (float)((colorCode & 0x00FF0000) >> 16 ) / (float)(0xFF);
}
static float GetB(int colorCode) {
    return (float)((colorCode & 0x0000FF00) >> 8  ) / (float)(0xFF);
}
static float GetA(int alphaCode) {
    return ((float)alphaCode / (float)0xFF);
}

static ImVec4 GetColor(int c, int a = Alpha80) {
    return ImVec4(GetR(c), GetG(c), GetB(c), GetA(a));
}
static ImVec4 Darken(const ImVec4 &c, float p) {
    return ImVec4(fmax(0., c.x - 1. * p), fmax(0., c.y - 1. * p), fmax(0., c.z - 1. *p), c.w);
}
static ImVec4 Lighten(const ImVec4 &c, float p) {
    return ImVec4(fmax(0., c.x + 1. * p), fmax(0., c.y + 1. * p), fmax(0., c.z + 1. *p), c.w);
}

static ImVec4 Disabled(const ImVec4 &c) {
    return Darken(c, 0.6);
}
static ImVec4 Hovered(const ImVec4 &c) {
    return Lighten(c, 0.2);
}
static ImVec4 Active(const ImVec4 &c) {
    return Lighten(ImVec4(c.x, c.y, c.z, 1.), 0.1);
}
static ImVec4 Collapsed(const ImVec4 &c) {
    return Darken(c, 0.2);
}


/**
 * Applies the game UI style.
 */
void GameUI::pushGameStyles() {
    // default font
    ImGui::PushFont(this->getFont(kGameFontRegular));

    // colors
    const static int BackGroundColor = 0x1F2421FF;
    const static int TextColor = 0xFCFCFCFF;
    const static int MainColor = 0x3F4B3BFF;
    const static int MainAccentColor = 0x5A9367FF;
    const static int HighlightColor = 0xFFC857FF;

    ImVec4* colors = ImGui::GetStyle().Colors;

    colors[ImGuiCol_Text]                       = GetColor(TextColor);
    colors[ImGuiCol_TextDisabled]               = Disabled(colors[ImGuiCol_Text]);
    colors[ImGuiCol_WindowBg]                   = GetColor(BackGroundColor, AlphaFull);
    colors[ImGuiCol_ChildBg]                    = GetColor(Black, Alpha20);
    colors[ImGuiCol_PopupBg]                    = GetColor(BackGroundColor, Alpha80);
    colors[ImGuiCol_Border]                     = Lighten(GetColor(BackGroundColor),0.4f);
    colors[ImGuiCol_BorderShadow]               = GetColor(Black);
    colors[ImGuiCol_FrameBg]                    = GetColor(MainAccentColor, Alpha50);
    colors[ImGuiCol_FrameBgHovered]             = Hovered(colors[ImGuiCol_FrameBg]);
    colors[ImGuiCol_FrameBgActive]              = Active(colors[ImGuiCol_FrameBg]);
    colors[ImGuiCol_TitleBg]                    = GetColor(BackGroundColor, Alpha90);
    colors[ImGuiCol_TitleBgActive]              = Active(colors[ImGuiCol_TitleBg]);
    colors[ImGuiCol_TitleBgCollapsed]           = Collapsed(colors[ImGuiCol_TitleBg]);
    colors[ImGuiCol_MenuBarBg]                  = Darken(GetColor(BackGroundColor), 0.2f);
    colors[ImGuiCol_ScrollbarBg]                = Lighten(GetColor(BackGroundColor, AlphaTransparent), 0.4f);
    colors[ImGuiCol_ScrollbarGrab]              = Lighten(GetColor(BackGroundColor), 0.3f);
    colors[ImGuiCol_ScrollbarGrabHovered]       = Hovered(colors[ImGuiCol_ScrollbarGrab]);
    colors[ImGuiCol_ScrollbarGrabActive]        = Active(colors[ImGuiCol_ScrollbarGrab]);
    colors[ImGuiCol_CheckMark]                  = GetColor(HighlightColor);
    colors[ImGuiCol_SliderGrab]                 = GetColor(HighlightColor);
    colors[ImGuiCol_SliderGrabActive]           = Active(colors[ImGuiCol_SliderGrab]);
    colors[ImGuiCol_Button]                     = GetColor(MainColor, Alpha80);
    colors[ImGuiCol_ButtonHovered]              = Hovered(colors[ImGuiCol_Button]);
    colors[ImGuiCol_ButtonActive]               = Active(colors[ImGuiCol_Button]);
    colors[ImGuiCol_Header]                     = GetColor(MainAccentColor, Alpha80);
    colors[ImGuiCol_HeaderHovered]              = Hovered(colors[ImGuiCol_Header]);
    colors[ImGuiCol_HeaderActive]               = Active(colors[ImGuiCol_Header]);
    colors[ImGuiCol_Separator]                  = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]           = Hovered(colors[ImGuiCol_Separator]);
    colors[ImGuiCol_SeparatorActive]            = Active(colors[ImGuiCol_Separator]);
    colors[ImGuiCol_ResizeGrip]                 = GetColor(MainColor, Alpha20);
    colors[ImGuiCol_ResizeGripHovered]          = Hovered(colors[ImGuiCol_ResizeGrip]);
    colors[ImGuiCol_ResizeGripActive]           = Active(colors[ImGuiCol_ResizeGrip]);
    colors[ImGuiCol_Tab]                        = GetColor(MainColor, Alpha60);
    colors[ImGuiCol_TabHovered]                 = Hovered(colors[ImGuiCol_Tab]);
    colors[ImGuiCol_TabActive]                  = Active(colors[ImGuiCol_Tab]);
    colors[ImGuiCol_TabUnfocused]               = colors[ImGuiCol_Tab];
    colors[ImGuiCol_TabUnfocusedActive]         = colors[ImGuiCol_TabActive];
    // colors[ImGuiCol_DockingPreview]          = Darken(colors[ImGuiCol_HeaderActive], 0.2f);
    // colors[ImGuiCol_DockingEmptyBg]          = Darken(colors[ImGuiCol_HeaderActive], 0.6f);
    colors[ImGuiCol_PlotLines]                  = GetColor(HighlightColor);
    colors[ImGuiCol_PlotLinesHovered]           = Hovered(colors[ImGuiCol_PlotLines]);
    colors[ImGuiCol_PlotHistogram]              = GetColor(HighlightColor);
    colors[ImGuiCol_PlotHistogramHovered]       = Hovered(colors[ImGuiCol_PlotHistogram]);
    colors[ImGuiCol_TextSelectedBg]             = GetColor(HighlightColor, Alpha40);
    colors[ImGuiCol_DragDropTarget]             = GetColor(HighlightColor, Alpha80);;
    colors[ImGuiCol_NavHighlight]               = GetColor(White);
    colors[ImGuiCol_NavWindowingHighlight]      = GetColor(White, Alpha80);
    colors[ImGuiCol_NavWindowingDimBg]          = GetColor(White, Alpha20);
    colors[ImGuiCol_ModalWindowDimBg]           = GetColor(Black, Alpha60);

#ifdef APPLE
    ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_Left;
#else
    ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_Right;
#endif

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 1.5);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, glm::vec2(.5, .5));
}

/**
 * Removes all styles we added.
 */
void GameUI::popGameStyles() {
    ImGui::PopFont();
    ImGui::PopStyleVar(3);
}
