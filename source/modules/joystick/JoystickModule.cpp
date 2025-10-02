#include "driver/EventQueue.hpp"
#include "modules/joystick/JoystickModule.hpp"

namespace love
{
    JoystickModule::JoystickModule() : Module(M_JOYSTICK, "love.joystick")
    {
        int joystickCount = joystick::getJoystickCount();
        
        // Use a simple file write for debug since printf might not show up
        FILE* debugFile = fopen("/vol/external01/wiiu/apps/balatro/joystick_debug.txt", "w");
        if (debugFile) {
            fprintf(debugFile, "JoystickModule: Detected %d joystick(s)\n", joystickCount);
            fclose(debugFile);
        }
        
        for (size_t index = 0; index < (size_t)joystickCount; index++)
        {
            debugFile = fopen("/vol/external01/wiiu/apps/balatro/joystick_debug.txt", "a");
            if (debugFile) {
                fprintf(debugFile, "JoystickModule: Adding joystick %zu\n", index);
                fclose(debugFile);
            }
            
            this->addJoystick(index);
            EventQueue::getInstance().sendJoystickStatus(true, index);
            
            debugFile = fopen("/vol/external01/wiiu/apps/balatro/joystick_debug.txt", "a");
            if (debugFile) {
                fprintf(debugFile, "JoystickModule: Successfully added joystick %zu\n", index);
                fclose(debugFile);
            }
        }
    }

    JoystickModule::~JoystickModule()
    {
        for (JoystickBase* joystick : this->joysticks)
        {
            joystick->close();
            joystick->release();
        }
    }

    JoystickBase* JoystickModule::getJoystick(int index)
    {
        if (index < 0 || (size_t)index >= this->activeSticks.size())
            return nullptr;

        return this->activeSticks[index];
    }

    int JoystickModule::getIndex(const JoystickBase* joystick)
    {
        for (int i = 0; i < (int)this->activeSticks.size(); i++)
        {
            if (this->activeSticks[i] == joystick)
                return i;
        }

        return -1;
    }

    int JoystickModule::getJoystickCount() const
    {
        return (int)this->activeSticks.size();
    }

    JoystickBase* JoystickModule::getJoystickFromID(int instanceId)
    {
        for (auto* joystick : this->joysticks)
        {
            if (joystick->getInstanceID() == instanceId)
                return joystick;
        }

        return nullptr;
    }

    JoystickBase* JoystickModule::addJoystick(int64_t deviceId)
    {
        if (deviceId < 0 || (int)deviceId >= joystick::getJoystickCount())
            return nullptr;

        std::string guid       = this->getDeviceGUID(deviceId);
        JoystickBase* joystick = nullptr;
        bool reused            = false;

        for (auto* stick : this->joysticks)
        {
            if (!stick->isConnected() && stick->getGUID() == guid)
            {
                joystick = stick;
                reused   = true;
                break;
            }
        }

        if (!joystick)
        {
            joystick = love::joystick::openJoystick(this->joysticks.size());
            this->joysticks.push_back(joystick);
        }

        this->removeJoystick(joystick);

        if (!joystick->open(deviceId))
            return nullptr;

        /*
        ** LÖVE checks for if multiple instances of a joystick
        ** are in the active list, and if so, it removes the
        ** one we just newly constructed and returns the active one.
        **
        ** We don't care. It shouldn't be possible.
        */

        if (joystick->isGamepad())
            this->recentGamepadGUIDs[joystick->getGUID()] = true;

        this->activeSticks.push_back(joystick);
        return joystick;
    }

    void JoystickModule::removeJoystick(JoystickBase* joystick)
    {
        if (!joystick)
            return;

        auto iterator = std::find(this->activeSticks.begin(), this->activeSticks.end(), joystick);

        if (iterator != this->activeSticks.end())
        {
            (*iterator)->close();
            this->activeSticks.erase(iterator);
        }
    }

    std::string JoystickModule::getDeviceGUID(int64_t deviceId) const
    {
        int index = (int)deviceId;
        if (index < 0 || index >= (int)this->activeSticks.size())
            return std::string();

        return this->activeSticks[index]->getGUID();
    }
} // namespace love
