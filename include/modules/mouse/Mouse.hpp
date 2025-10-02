// This header acts as a thin router: on Wii U we use the platform-specific Mouse
// definition with extended features (touch/Wiimote). On other platforms we fall
// back to the minimal generic Mouse implementation.

#pragma once

#if defined(NINTENDO_WIIU)

// For Wii U builds, the platform-specific header with the same include guard
// path (modules/mouse/Mouse.hpp) is found first via include directories.
// This file still needs to exist for non-Wii U builds.

#else

#include "common/Module.hpp"

namespace love
{
    class Mouse : public Module
    {
      public:
        Mouse();

        virtual ~Mouse() {}

        void getPosition(double& x, double& y) const;

        void setPosition(double x, double y);

        bool isVisible() const;

        void setVisible(bool visible);

        bool isGrabbed() const;

        void setGrabbed(bool grabbed);

        bool getRelativeMode() const;

        void setRelativeMode(bool relative);

      private:
        double x = 0.0;
        double y = 0.0;
        bool visible = true;
        bool grabbed = false;
        bool relativeMode = false;
    };
} // namespace love

#endif // NINTENDO_WIIU
