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
class TitleScreen;
}

namespace gui::title {
class WorldSelector: public gui::GameWindow {
    public:
        WorldSelector(TitleScreen *title);
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
        void createWorld(const std::string &, const bool = true);

        void drawErrors(gui::GameUI *);
        void drawRecentsList(gui::GameUI *);
        void drawCreate(gui::GameUI *);

        void setError(const std::string &path, const std::string &detail);

        /// file dialog filter string for world files
        constexpr static const char *kWorldFilters = "v1 World (.world){.world}";
        /// maximum characters for world names
        constexpr static const size_t kNameMaxLen = 128;

    private:
        // title screen instance holds the fun methods for actually changing game modes
        TitleScreen *title = nullptr;

        // recents data, as loaded from prefs (if there is any)
        Recents recents;
        // whether a file dialog is open
        bool isFileDialogOpen = false;

        // selected world
        int selectedWorld = -1;

        // error message open?
        bool isErrorOpen = false;
        // filename to display for error message
        std::string errorFile;
        // error message detail text
        std::string errorDetail;

        // world creation modal open?
        bool isCreateOpen = false;
        // name for new world
        char newName[kNameMaxLen] = {0};
        // seed for new world
        int newSeed = 420;
};
}

#endif
