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

    public:
        void startFrame();
        bool handleEvent(const SDL_Event &);

        /**
         * Gets the Euler angles.
         */
        glm::vec3 getEulerAngles() const {
            return this->eulerAngles;
        }

        /**
         * Gets the velocity with which the camera should move in each of
         * the three axes.
         */
        glm::vec3 getMovementDelta() const {
            return this->movementDelta;
        }

        void incrementCursorCount();
        void decrementCursorCount();

        bool acceptsGameInput() const {
            return this->inputUpdatesCamera;
        }

    private:
        enum Keys {
            KeyMoveLeft     = 0,
            KeyMoveRight    = 1,
            KeyMoveFront    = 2,
            KeyMoveBack     = 3,

            KeyMoveUp       = 4,
            KeyMoveDown     = 5,
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
        glm::vec3 eulerAngles;

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
        bool inputUpdatesCamera = false;
        // when set, the input debug view is visible
        bool showDebugWindow = true;
        // when set, the profiler window is shown
        bool showProfiler = false;

        // main window handle (for adjusting mouse behavior)
        gui::MainWindow *window = nullptr;

        // cursor reference count; when 0, no mouse cursor is shown
        size_t cursorRefCount = 0;

    private:
        double mouseDeltaX = 0., mouseDeltaY = 0.;
        double moveDeltaX = 0., moveDeltaZ = 0.; // Z is front/back, X is left/right
        double moveDeltaY = 0.;

        std::bitset<32> keys;
};
}

#endif
