#ifndef INVENTORY_UIDETAIL_H
#define INVENTORY_UIDETAIL_H

namespace gui {
class GameUI;
}

namespace inventory {
class UI;

class UIDetail {
    public:
        UIDetail(UI *_owner) : owner(_owner) {};

        void draw(gui::GameUI *gui);

    private:

    private:
        UI *owner = nullptr;
};
}

#endif
