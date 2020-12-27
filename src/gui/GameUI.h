#ifndef GUI_GAMEUI_H
#define GUI_GAMEUI_H

#include "RunLoopStep.h"
#include "GameWindow.h"

#include <string>
#include <vector>
#include <unordered_map>

struct ImFont;

struct SDL_Window;
union SDL_Event;

namespace gui {
class GameUI: public RunLoopStep {
    public:
        GameUI(SDL_Window *win, void *ctx);
        ~GameUI();

    public:
        void willBeginFrame();
        void draw();

        bool handleEvent(const SDL_Event &);

        void reshape(unsigned int width, unsigned int height);

        /// Gets the handle to a loaded font by name, or nil
        ImFont *getFont(const std::string &name) {
            return this->fonts[name];
        }

        /// changes the GUI scale
        void setGuiScale(float scale) {
            this->scale = scale;
            this->loadFonts(1.);
        }

        void addWindow(std::shared_ptr<GameWindow> window);
        void removeWindow(std::shared_ptr<GameWindow> window);

    public:
        static const std::string kRegularFontName;
        static const std::string kBoldFontName;
        static const std::string kItalicFontName;
        static const std::string kMonospacedFontName;
        static const std::string kMonospacedBoldFontName;

        static const std::string kGameFontRegular;
        static const std::string kGameFontBold;

    private:
        void loadFonts(const double scale);

        void pushGameStyles();
        void popGameStyles();

    private:
        struct FontInfo {
            std::string path;
            std::string name;

            bool scalesWithUi = false;
            double size = 15;
        };

        static const std::vector<FontInfo> kDefaultFonts;

    private:
        // SDL main window
        SDL_Window *window = nullptr;

        // name -> font for all loaded fonts
        std::unordered_map<std::string, ImFont *> fonts;
        // windows to render during the frame
        std::vector<std::shared_ptr<GameWindow>> windows;

        // scale factor for all in game UI
        float scale = 1.5f;
};
}

#endif
