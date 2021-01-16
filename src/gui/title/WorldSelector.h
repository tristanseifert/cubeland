#ifndef GUI_TITLE_WORLDSELECTOR_H
#define GUI_TITLE_WORLDSELECTOR_H

#include "gui/GameWindow.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <string>
#include <variant>
#include <vector>

#include <avir.h>
#include <lancir.h>
#include <blockingconcurrentqueue.h>

#include <glm/vec2.hpp>

namespace gui {
class GameUI;
class TitleScreen;
}

namespace gui::title {
class WorldSelector: public gui::GameWindow {
    public:
        WorldSelector(TitleScreen *title);
        virtual ~WorldSelector();

        void loadRecents();

        void startOfFrame();
        void draw(gui::GameUI *) override;

        bool skipDrawIfInvisible() const override {
            return false;
        }

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

        // indicates a new world has been selected
        struct WorldSelection {
            /// path to world file
            std::string path;
        };

        // info on a background image to update
        struct BgImageInfo {
            bool valid = false;
            std::vector<std::byte> data;
            glm::ivec2 size;
        };

        using WorkItem = std::variant<std::monostate, WorldSelection>;

    private:
        void saveRecents();

        void openWorld(const std::string &);
        void createWorld(const std::string &, const bool = true);

        void drawErrors(gui::GameUI *);
        void drawRecentsList(gui::GameUI *);
        void drawCreate(gui::GameUI *);

        void setError(const std::string &path, const std::string &detail);

        void updateSelectionThumb();

        void workerMain();
        void workerSelectionChanged(const WorldSelection &);
        bool decodeImage(const std::filesystem::path &, std::vector<std::byte> &, glm::ivec2 &);

        /// file dialog filter string for world files
        constexpr static const char *kWorldFilters = "v1 World (.world){.world}";
        /// maximum characters for world names
        constexpr static const size_t kNameMaxLen = 128;

        /// blur radius for level backgrounds
        constexpr static const size_t kBgBlurRadius = 15.;

    private:
        std::unique_ptr<std::thread> worker = nullptr;
        std::atomic_bool workerRun;
        moodycamel::BlockingConcurrentQueue<WorkItem> work;

        /// shared image resizer
        //avir::CImageResizer<> imgResizer = avir::CImageResizer<>(8);
        avir::CLancIR imgResizer;
        /// downscaling factor for preview images
        // TODO: set automatically based on HiDPI setting
        float previewScaleFactor = 3.;

        /// info of a background image to upload
        std::optional<BgImageInfo> backgroundInfo = std::nullopt;

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

        // last frame's visibility state
        bool lastVisible = false;
};
}

#endif
