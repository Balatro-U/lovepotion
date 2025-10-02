// Wii U KPAD joystick implementation (diagnostic stub)
#ifdef __WIIU__
#include <cstring>
#include <vector>
#include <cstdint>
#include <padscore/kpad.h>
#include "modules/joystick/kpad/Joystick.hpp"
#include "DebugLogger.hpp"
#endif

#ifdef __WIIU__
#include <cstdio>
static void joyKLog(const char* fmt,...){
    FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
    if(f){ va_list ap; va_start(ap,fmt); vfprintf(f,fmt,ap); va_end(ap); fputc('\n',f); fflush(f); fclose(f);} 
    va_list ap2; va_start(ap2,fmt); vprintf(fmt,ap2); va_end(ap2); printf("\n"); fflush(stdout);
}
#endif

namespace love {
    namespace kpad {
#ifdef __WIIU__
        Joystick::Joystick(int id) : JoystickBase(id)
        {
            joyKLog("[KPAD JOYSTICK] ctor(id=%d) begin this=%p", id, (void*)this);
            joyKLog("[KPAD JOYSTICK] ctor(id=%d) end", id);
        }

        Joystick::Joystick(int id, int index) : JoystickBase(id, index)
        {
            joyKLog("[KPAD JOYSTICK] ctor(id=%d,index=%d) begin this=%p", id, index, (void*)this);
            this->open(index);
            joyKLog("[KPAD JOYSTICK] ctor(id=%d,index=%d) end", id, index);
        }

        Joystick::~Joystick()
        {
            this->close();
        }

        bool Joystick::open(int64_t deviceId)
        {
            joyKLog("[KPAD JOYSTICK] open(deviceId=%lld)", (long long)deviceId);
            this->close();
            this->instanceId   = 0;
            this->gamepadType  = GAMEPAD_TYPE_NINTENDO_WII_REMOTE;
            this->guid         = love::getGamepadGUID(this->gamepadType);
            if (!Joystick::getConstant(this->gamepadType, this->name))
                this->name = "Nintendo Wii Remote (stub)";
            this->joystickType = JOYSTICK_TYPE_GAMEPAD;
            std::memset(&this->status, 0, sizeof(this->status));
            std::memset(&this->state, 0, sizeof(this->state));
            return true;
        }

        void Joystick::close()
        {
            joyKLog("[KPAD JOYSTICK] close() instanceId=%lld", (long long)this->instanceId);
            this->instanceId = -1;
        }

        void Joystick::update()
        {
            std::memset(&this->state, 0, sizeof(this->state));
#ifdef KPAD_CHAN_0
            // Real read left disabled until proper mapping is implemented.
            if (KPADRead(KPAD_CHAN_0, &this->status, 1) == 0) {
                // propagate button bitfields
                this->state.pressed  = this->status.trigger;
                this->state.released = this->status.release;
                this->state.held     = this->status.hold;
            }
#endif
        }

        bool Joystick::isConnected() const
        {
            return this->instanceId >= 0; // Always true after open in stub
        }

        float Joystick::getAxis(GamepadAxis) const
        {
            return 0.0f; // No axis support yet
        }

        std::vector<float> Joystick::getAxes() const
        {
            std::vector<float> axes;
            axes.assign(this->getAxisCount(), 0.0f);
            return axes;
        }

        bool Joystick::isDown(std::span<GamepadButton> /*buttons*/) const
        {
            return false;
        }

        bool Joystick::isHeld(std::span<GamepadButton> /*buttons*/) const
        {
            return false;
        }

        bool Joystick::isUp(std::span<GamepadButton> /*buttons*/) const
        {
            return true;
        }

        bool Joystick::isAxisChanged(GamepadAxis /*axis*/) const
        {
            return false;
        }

        void Joystick::setPlayerIndex(int /*index*/)
        {
            // Not tracked in stub
        }

        int Joystick::getPlayerIndex() const
        {
            return 0;
        }

        Joystick::JoystickInput Joystick::getGamepadMapping(const GamepadInput& /*input*/) const
        {
            JoystickInput ji{};
            ji.type = INPUT_TYPE_AXIS; // choose axis type; axis field default 0
            ji.axis = 0;
            return ji;
        }

        std::string Joystick::getGamepadMappingString() const
        {
            return std::string();
        }

        bool Joystick::isVibrationSupported() const
        {
            return false;
        }

        bool Joystick::setVibration(float /*left*/, float /*right*/, float /*duration*/)
        {
            return false;
        }

        bool Joystick::setVibration()
        {
            return false;
        }

        void Joystick::getVibration(float& left, float& right) const
        {
            left = right = 0.0f;
        }

        bool Joystick::hasSensor(Sensor::SensorType /*type*/) const
        {
            return false;
        }

        bool Joystick::isSensorEnabled(Sensor::SensorType /*type*/) const
        {
            return false;
        }

        void Joystick::setSensorEnabled(Sensor::SensorType /*type*/, bool /*enable*/)
        {
            // no-op
        }

        std::vector<float> Joystick::getSensorData(Sensor::SensorType /*type*/) const
        {
            return {}; // empty
        }

        Vector2 Joystick::getPosition() const
        {
            return {0.0f, 0.0f};
        }

        Vector2 Joystick::getAngle() const
        {
            return {0.0f, 0.0f};
        }
#endif // __WIIU__
    } // namespace kpad
} // namespace love
