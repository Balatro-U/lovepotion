#include "modules/mouse/Mouse.hpp"

#include <vpad/input.h>
#include <padscore/kpad.h>
#include <cstring>

#ifdef __WIIU__
#include "DebugLogger.hpp"
#include <cstdio>
static void mouseLog(const char* fmt,...){
    FILE* f = fopen("/vol/external01/wiiu/apps/balatro/simple_debug.log","a");
    if(f){
        va_list ap; va_start(ap,fmt); vfprintf(f,fmt,ap); va_end(ap); fputc('\n',f); fflush(f); fclose(f);
    }
    // Mirror to console (may not show but harmless)
    va_list ap2; va_start(ap2,fmt); vprintf(fmt,ap2); va_end(ap2); printf("\n"); fflush(stdout);
}
#endif

namespace love
{
    Mouse::Mouse() : MouseBase()
    {
#ifdef __WIIU__
        mouseLog("[MOUSE] Constructor begin this=%p", (void*)this);
#endif
        // Initialize touch data
        touchData.x = 0;
        touchData.y = 0;
        touchData.touched = 0;
        
        // Initialize Wiimote data
        for(int i = 0; i < 4; i++)
        {
            wiimoteConnected[i] = false;
            memset(&wiimoteStatus[i], 0, sizeof(KPADStatus));
            memset(&prevWiimoteStatus[i], 0, sizeof(KPADStatus));
        }
        wiimoteActiveCount = 0;
        
        // Initialize KPAD for Wiimote support
        KPADInit();
#ifdef __WIIU__
        mouseLog("[MOUSE] Constructor end");
#endif
    }

    // Destructor is trivial; defined inline in the header.

    void Mouse::getPosition(double& x, double& y) const
    {
        x = this->x;
        y = this->y;
    }

    void Mouse::setPosition(double x, double y)
    {
        this->x = x;
        this->y = y;
    }

    bool Mouse::isVisible() const
    {
        return this->visible;
    }

    void Mouse::setVisible(bool visible)
    {
        this->visible = visible;
    }

    bool Mouse::isGrabbed() const
    {
        return this->grabbed;
    }

    void Mouse::setGrabbed(bool grabbed)
    {
        this->grabbed = grabbed;
    }

    bool Mouse::getRelativeMode() const
    {
        return this->relativeMode;
    }

    void Mouse::setRelativeMode(bool relative)
    {
        this->relativeMode = relative;
    }

    void Mouse::updateTouchPosition()
    {
        // On Wii U, we can use the GamePad touch screen as mouse input
        VPADStatus vpadStatus;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &error);
        
        if (error == VPAD_READ_SUCCESS && vpadStatus.tpNormal.touched)
        {
            // Convert touch coordinates to screen coordinates
            // GamePad touch screen is 854x480
            this->x = (double)vpadStatus.tpNormal.x;
            this->y = (double)vpadStatus.tpNormal.y;
        }
        
        // Also update Wiimote input
        updateWiimoteInput();
    }

    void Mouse::updateWiimoteInput()
    {
        // Store previous state
        for(int i = 0; i < 4; i++)
        {
            prevWiimoteStatus[i] = wiimoteStatus[i];
        }
        
        // Read current Wiimote states
        wiimoteActiveCount = 0;
        for(int i = 0; i < 4; i++)
        {
            if(KPADRead((KPADChan)i, &wiimoteStatus[i], 1) > 0)
            {
                wiimoteConnected[i] = true;
                wiimoteActiveCount++;
                
                // Log first connection or button presses
                static bool logged_connection = false;
                if (!logged_connection || wiimoteStatus[i].hold != 0) {
                    printf("[WIIMOTE] Controller %d: hold=0x%08X, pos=(%.3f,%.3f)\n", 
                           i, wiimoteStatus[i].hold, wiimoteStatus[i].pos.x, wiimoteStatus[i].pos.y);
                    logged_connection = true;
                }
                
                // Update mouse position based on IR pointing
                if(wiimoteStatus[i].extensionType == WPAD_EXT_CORE || 
                   wiimoteStatus[i].extensionType == WPAD_EXT_NUNCHUK)
                {
                    // Check if IR is valid
                    if(wiimoteStatus[i].pos.x >= 0.0f && wiimoteStatus[i].pos.x <= 1.0f &&
                       wiimoteStatus[i].pos.y >= 0.0f && wiimoteStatus[i].pos.y <= 1.0f)
                    {
                        // Convert IR coordinates to screen coordinates
                        // TV screen is typically 1280x720, but we'll use normalized coordinates
                        this->x = wiimoteStatus[i].pos.x * 1280.0;
                        this->y = wiimoteStatus[i].pos.y * 720.0;
                    }
                }
            }
            else
            {
                wiimoteConnected[i] = false;
            }
        }
    }

    bool Mouse::isWiimoteButtonDown(int button) const
    {
        // Check if any connected Wiimote has the button pressed
        for(int i = 0; i < 4; i++)
        {
            if(wiimoteConnected[i])
            {
                switch(button)
                {
                    case 1: // Left click (A button)
                        if(wiimoteStatus[i].trigger & WPAD_BUTTON_A)
                            return true;
                        break;
                    case 2: // Right click (B button)
                        if(wiimoteStatus[i].trigger & WPAD_BUTTON_B)
                            return true;
                        break;
                }
            }
        }
        return false;
    }

    bool Mouse::wasWiimoteButtonPressed(int button) const
    {
        // Check if button was just pressed (not held)
        for(int i = 0; i < 4; i++)
        {
            if(wiimoteConnected[i])
            {
                bool currentPressed = false;
                bool previousPressed = false;
                
                switch(button)
                {
                    case 1: // Left click (A button)
                        currentPressed = (wiimoteStatus[i].trigger & WPAD_BUTTON_A) != 0;
                        previousPressed = (prevWiimoteStatus[i].trigger & WPAD_BUTTON_A) != 0;
                        break;
                    case 2: // Right click (B button)
                        currentPressed = (wiimoteStatus[i].trigger & WPAD_BUTTON_B) != 0;
                        previousPressed = (prevWiimoteStatus[i].trigger & WPAD_BUTTON_B) != 0;
                        break;
                }
                
                if(currentPressed && !previousPressed)
                    return true;
            }
        }
        return false;
    }

    bool Mouse::wasWiimoteButtonReleased(int button) const
    {
        // Check if button was just released
        for(int i = 0; i < 4; i++)
        {
            if(wiimoteConnected[i])
            {
                bool currentPressed = false;
                bool previousPressed = false;
                
                switch(button)
                {
                    case 1: // Left click (A button)
                        currentPressed = (wiimoteStatus[i].trigger & WPAD_BUTTON_A) != 0;
                        previousPressed = (prevWiimoteStatus[i].trigger & WPAD_BUTTON_A) != 0;
                        break;
                    case 2: // Right click (B button)
                        currentPressed = (wiimoteStatus[i].trigger & WPAD_BUTTON_B) != 0;
                        previousPressed = (prevWiimoteStatus[i].trigger & WPAD_BUTTON_B) != 0;
                        break;
                }
                
                if(!currentPressed && previousPressed)
                    return true;
            }
        }
        return false;
    }
} // namespace love
