/**
 * Handles consuming keyboard/mouse (or game controller) events in order to drive the update of the
 * camera.
 */
#ifndef INPUT_INPUT_INPUTMANAGER_H
#define INPUT_INPUT_INPUTMANAGER_H

#include <glm/vec3.hpp>

#include <bitset>

union SDL_Event;

namespace input {
class InputManager {
    public:
        InputManager();

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
        double pitch = 0.;
        double yaw = 0.;
        double roll = 0.;

        /**
         * A vector containing the distance that the camera should move, in each of the X, Y and 
         * Z axes.
         */
        glm::vec3 movementDelta;

        /**
         * Camera look sensitivity. This serves as a multiplier on the basic angle value delta.
         */
        double lookSensitivity = 0.05;

        /**
         * When set, the up/down movement is reversed.
         */
        bool reverseLookUpDown = false;

        /**
         * Player movement sensitivity. It multiplies the basic movement delta.
         */
        double movementSensitivity = 0.125;

    private:
        double mouseDeltaX = 0., mouseDeltaY = 0.;
        double moveDeltaX = 0., moveDeltaZ = 0.; // Z is front/back, X is left/right
        double moveDeltaY = 0.;

        std::bitset<32> keys;
};
}

#endif
