#include "modules/timer/Timer.hpp"

#include <cmath>
#include <coreinit/thread.h>

namespace love
{
    OSTick Timer::reference = 0;

    Timer::Timer()
    {
        Timer::reference    = OSGetTime();
        this->prevFpsUpdate = this->currTime = Timer::getTime();
    }

    double Timer::getTime()
    {
        auto now = OSGetTime();

        if (now < Timer::reference)
            Timer::reference = now;

        const auto ns = OSTicksToNanoseconds(now - Timer::reference);
        return static_cast<double>(ns) / Timer::SECONDS_TO_NS;
    }

    void Timer::sleep(double seconds) const
    {
        const auto time        = std::chrono::duration<double>(seconds);
        const auto nanoseconds = std::chrono::duration<double, std::nano>(time).count();

        OSSleepTicks(OSNanosecondsToTicks(nanoseconds));
    }

    double Timer::step()
    {
        constexpr double MAX_FRAME_DT = 0.25;

        this->frames++;

        this->prevTime = this->currTime;
        double measuredTime = Timer::getTime();
        double measuredDt   = measuredTime - this->prevTime;

        if (!std::isfinite(measuredDt) || measuredDt < 0.0)
        {
            measuredTime = this->prevTime;
            measuredDt   = 0.0;
        }
        else if (measuredDt > MAX_FRAME_DT)
        {
            measuredDt = MAX_FRAME_DT;
        }

        this->currTime = measuredTime;
        this->dt       = measuredDt;

        double timeSinceLast = (this->currTime - this->prevFpsUpdate);

        if (timeSinceLast > this->fpsUpdateFrequency)
        {
            this->fps           = int((this->frames / timeSinceLast) + 0.5);
            this->averageDelta  = timeSinceLast / frames;
            this->prevFpsUpdate = this->currTime;
            this->frames        = 0;
        }

        return this->dt;
    }
} // namespace love
