#ifndef GUI_GAMEUI_H
#define GUI_GAMEUI_H

#include "GameWindow.h"

#include <string>
#include <vector>

struct SDL_Window;
union SDL_Event;

namespace gui {
class GameUI {
    public:
        GameUI(SDL_Window *win, void *ctx);
        ~GameUI();

    public:
        void willBeginFrame();
        void draw();

        bool handleEvent(const SDL_Event &);

    private:
        void loadFonts(const double scale);

    private:
        struct FontInfo {
            std::string path;
            std::string name;

            double size = 15;
        };

        static const std::vector<FontInfo> kDefaultFonts;

    private:
        SDL_Window *window = nullptr;

        std::vector<std::shared_ptr<GameWindow>> windows;

};
}

#endif
