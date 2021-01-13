#ifndef GUI_TITLE_WORLDSELECTOR_H
#define GUI_TITLE_WORLDSELECTOR_H

#include "gui/GameWindow.h"

#include <chrono>
#include <string>
#include <optional>
#include <array>
#include <cstddef>

namespace gui {
class GameUI;
}

namespace gui::title {
class WorldSelector: public gui::GameWindow {
    public:
        WorldSelector();
        virtual ~WorldSelector() = default;

        void loadRecents();

        void draw(gui::GameUI *) override;

    private:
        static const std::string kPrefsKey;

        /// Entry in the recents list
        struct RecentsEntry {
            /// file path
            std::string path;
            /// last opened timestamp
            std::chrono::system_clock::time_point lastOpened;

            /// Creates a new recents entry with the current time.
            RecentsEntry(const std::string &_path) : path(_path) {
                this->lastOpened = std::chrono::system_clock::now();
            }
            /// Creates an uninitialized recents entry
            RecentsEntry() = default;

            template<class Archive> void serialize(Archive &arc) {
                arc(this->path);
                arc(this->lastOpened);
            }
        };

        /// Recents list
        struct Recents {
            constexpr static const size_t kMaxRecents = 10;

            /// Recents entries (or nullopt)
            std::array<std::optional<RecentsEntry>, kMaxRecents> recents;

            /// Determines whether any of the slots are filled
            const bool empty() const {
                for(size_t i = 0; i < kMaxRecents; i++) {
                    if(this->recents[i]) return false;
                }

                return true;
            }

            /// Adds the given path to the recents list, or sets the date of an existing one
            void addPath(const std::string &path) {
                using namespace std::chrono;

                // check if we exists
                for(size_t i = 0; i < kMaxRecents; i++) {
                    auto &entry = this->recents[i];
                    if(!entry) continue;

                    if(entry->path == path) {
                        entry->lastOpened = system_clock::now();
                        return;
                    }
                }

                // we don't, try to insert one in an empty slot
                RecentsEntry entry(path);
                for(size_t i = 0; i < kMaxRecents; i++) {
                    if(this->recents[i]) continue;

                    this->recents[i] = entry;
                    return;
                }

                // evict the oldest entry
                const auto now = system_clock::now();
                size_t secs = 0;
                size_t toRemove = 0;

                for(size_t i = 0; i < kMaxRecents; i++) {
                    auto entry = this->recents[i];
                    if(!entry) continue;

                    auto diff = duration_cast<seconds>(now - entry->lastOpened).count();
                    if(diff > secs) {
                        secs = diff;
                        toRemove = i;
                    }
                }

                this->recents[toRemove] = entry;
            }

            template<class Archive> void serialize(Archive &arc) {
                arc(this->recents);
            }
        };

    private:
        void saveRecents();

        void openWorld(const std::string &);

        void drawRecentsList(gui::GameUI *);

    private:
        // recents data, as loaded from prefs (if there is any)
        Recents recents;
        // whether a file dialog is open
        bool isFileDialogOpen = false;

        // selected world
        int selectedWorld = -1;
};
}

#endif
