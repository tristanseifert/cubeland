/**
 * Handles consuming keyboard/mouse (or game controller) events in order to drive the update of the
 * camera.
 */
#ifndef INPUT_INPUT_INPUTMANAGER_H
#define INPUT_INPUT_INPUTMANAGER_H

#include <glm/vec3.hpp>

#include <bitset>

union SDL_Event;

namespace gui {
class MainWindow;
}

namespace input {
class InputManager {
    friend class PlayerPosPersistence;

    public:
        InputManager(gui::MainWindow *);
        ~InputManager();

    public:
        void startFrame();
        bool handleEvent(const SDL_Event &);

        /**
         * Gets the Euler angles.
         */
        const glm::vec3 getEulerAngles() const {
            return this->eulerAngles;
        }
        /// Gets the non-pitched Euler angles.
        const glm::vec3 getNonpitchEulerAngles() const {
            return this->eulerAnglesNoPitch;
        }
    
        /// gets regular angles (pitch, yaw, roll) shoved into a vector
        const glm::vec3 getAngles() const {
            return glm::vec3(this->pitch, this->yaw, this->roll);
        }
        /// sets the regular angles from a vector
        void setAngles(const glm::vec3 &a) {
            this->pitch = a.x;
            this->yaw = a.y;
            this->roll = a.z;
        }
    
        /**
         * Gets the velocity with which the camera should move in each of
         * the three axes.
         */
        const glm::vec3 getMovementDelta() const {
            return this->movementDelta;
        }

        /// Returns whether we want to jump or not
        const bool shouldJump() const {
            return this->wantsJump;
        }

        void incrementCursorCount();
        void decrementCursorCount();

        const size_t getCursorCount() const {
            return this->cursorRefCount;
        }

        bool acceptsGameInput() const {
            return this->inputUpdatesCamera;
        }

    private:
        enum Keys {
            KeyMoveLeft         = 0,
            KeyMoveRight        = 1,
            KeyMoveFront        = 2,
            KeyMoveBack         = 3,

            KeyMoveUp           = 4,
            KeyMoveDown         = 5,

            KeyJump             = 6,
        };

    private:
        void handleKey(int, unsigned int, bool);

        void updateAngles();
        void updatePosition();

        void drawDebugWindow();

    private:
        /**
         * A vector containing the pitch, yaw, and roll angles, calculated from user input.
         * Depending on how the camera is configured, this will eventually get turned into the
         * look angle.
         */
        glm::vec3 eulerAngles, eulerAnglesNoPitch;

        /**
         * Pitch, yaw and roll values for the camera. They are in degrees.
         */
        float pitch = 0.;
        float yaw = 0.;
        float roll = 0.;

        /**
         * A vector containing the distance that the camera should move, in each of the X, Y and 
         * Z axes.
         */
        glm::vec3 movementDelta;

        /**
         * Camera look sensitivity. This serves as a multiplier on the basic angle value delta.
         */
        float lookSensitivity = 0.05;

        /**
         * When set, the up/down movement is reversed.
         */
        bool reverseLookUpDown = false;

        /**
         * Player movement sensitivity. It multiplies the basic movement delta.
         */
        float movementSensitivity = 0.15;

        // whether user input has any effect on the camera position
        bool inputUpdatesCamera = true;
        // when set, the input debug view is visible
        bool showDebugWindow = false;

        // main window handle (for adjusting mouse behavior)
        gui::MainWindow *window = nullptr;

        // cursor reference count; when 0, no mouse cursor is shown
        size_t cursorRefCount = 0;

        // whether we want to be jumping if not on the ground
        bool wantsJump = false;

        uint32_t debugMenuItem = 0;

    private:
        double mouseDeltaX = 0., mouseDeltaY = 0.;
        double moveDeltaX = 0., moveDeltaZ = 0.; // Z is front/back, X is left/right
        double moveDeltaY = 0.;

        std::bitset<32> keys;
};
}

#endif
