/*
 *
 * FrameClock is by Lee R and was originaly written for SFML
 *
 * Nikolaus Rauch altered the original version:
 *  Dependecy for SFML removed. Uses c++11 chrono timer instead.
 *
 *
 * Note : This is proof of concept and should be cleaned up (for instance static decleration in frameClockWindow of FrameClockGuiHelper)
 */

/*
//
// FrameClock - Utility class for tracking frame statistics.
// Copyright (C) 2013 Lee R (lee-r@outlook.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
*/

#pragma once

#include <chrono>
#include <limits>
#include <cassert>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>
#include <imgui.h>

class FrameClock;

void frameClockWindow(const FrameClock& clock);

namespace detail
{

template<typename T> inline T limitMax()
{
    return (std::numeric_limits<T>::max)();
}

template<typename T> inline T limitMin()
{
    return (std::numeric_limits<T>::min)();
}

template<typename T>
static std::string format(std::string name, std::string resolution, T value)
{
    std::ostringstream os;
    os.precision(4);
    os << std::setfill(' ');
    os << std::left << std::setw(5);
    os << name << " : ";
    os << std::setw(5);
    os << value << " " << resolution;
    return os.str();
}

template<typename T>
static std::string stringPrecision(T value, int precision = 4)
{
    std::ostringstream os;
    os.precision(precision);
    os << value;
    return os.str();
}

struct FrameClockGuiHelper
{
    FrameClockGuiHelper() : mPlotFPS(120, 0), mPlotFPSOffset(0), mFrameTime(150,0), mFrameTimeBins(150,0), mRefreshTime(1.0/60.0f), mCurrentRefreshTime(0.0f) {}

    std::vector<float> mPlotFPS;
    unsigned int mPlotFPSOffset;

    std::vector<float> mFrameTime;
    std::vector<float> mFrameTimeBins;

    float mRefreshTime;
    float mCurrentRefreshTime;
};

}

class FrameClock
{
private:
    typedef std::chrono::high_resolution_clock HighResClock;
    typedef HighResClock::time_point TimePoint;
    typedef HighResClock::duration Time;

    struct Clock {
        TimePoint timeStamp;
        Time elapsed;

        Clock()
        {
            timeStamp = HighResClock::now();
            elapsed = Time::zero();
        }

        Time restart()
        {
            auto pt = HighResClock::now();
            elapsed = pt - timeStamp;
            timeStamp = pt;

            return elapsed;
        }

        static inline float asSeconds(Time time) { return  std::chrono::duration<float, std::ratio<1>>(time).count() ;}
    };

public:
    FrameClock(std::size_t depth = 100)
    {
        assert(depth >= 1);

        sample.data.resize(depth);

        freq.minimum = detail::limitMax<float>();
        freq.maximum = 0.0f;
        time.minimum = Time::max();
        time.maximum = Time::zero();
    }

    inline void beginFrame()
    {
        clock.restart();
    }

    inline Time endFrame()
    {
        time.current = clock.elapsed;

        sample.accumulator -= sample.data[sample.index];
        sample.data[sample.index] = time.current;

        sample.accumulator += time.current;
        time.elapsed += time.current;

        if(++sample.index >= getSampleDepth())
            sample.index = 0;

        if(time.current > std::chrono::duration<float>(1.719e-04))
            freq.current = 1.0f / Clock::asSeconds(time.current);

        if(sample.accumulator != Time::zero()) {
            const float smooth = static_cast<float>(getSampleDepth());
            freq.average = smooth / Clock::asSeconds(sample.accumulator);
        }

        time.average = sample.accumulator / getSampleDepth();

        if (freq.current < freq.minimum)
            freq.minimum = freq.current;
        if (freq.current > freq.maximum)
            freq.maximum = freq.current;

        if (time.current < time.minimum)
            time.minimum = time.current;
        if (time.current > time.maximum)
            time.maximum = time.current;

        ++freq.elapsed;

        return time.current;
    }

    void clear()
    {
        FrameClock(getSampleDepth()).swap(*this);
    }

    void setSampleDepth(std::size_t depth)
    {
        assert(depth >= 1);
        FrameClock(depth).swap(*this);
    }

    inline std::size_t getSampleDepth() const
    {
        return sample.data.size();
    }

    inline float getTotalFrameTime() const
    {
        return Clock::asSeconds(time.elapsed);
    }

    inline std::uint64_t getTotalFrameCount() const
    {
        return freq.elapsed;
    }

    inline float getLastFrameTime() const
    {
        return Clock::asSeconds(time.current);
    }

    inline float getMinFrameTime() const
    {
        return Clock::asSeconds(time.minimum);
    }

    inline float getMaxFrameTime() const
    {
        return Clock::asSeconds(time.maximum);
    }

    inline float getAverageFrameTime() const
    {
        return Clock::asSeconds(time.average);
    }

    inline float getFramesPerSecond() const
    {
        return freq.current;
    }

    inline float getMinFramesPerSecond() const
    {
        return freq.minimum;
    }

    inline float getMaxFramesPerSecond() const
    {
        return freq.maximum;
    }

    inline float getAverageFramesPerSecond() const
    {
        return freq.average;
    }

    void swap(FrameClock& other)
    {
        time.swap(other.time);
        freq.swap(other.freq);
        sample.swap(other.sample);
        std::swap(clock, other.clock);
    }

private:

    template<typename T, typename U>
    struct Range {
        Range() :   minimum(),
            maximum(),
            average(),
            current(),
            elapsed()
        {}

        void swap(Range& other) {
            std::swap(minimum, other.minimum);
            std::swap(maximum, other.maximum);
            std::swap(average, other.average);
            std::swap(current, other.current);
            std::swap(elapsed, other.elapsed);
        }

        T minimum;
        T maximum;
        T average;
        T current;
        U elapsed;
    };

    struct SampleData {

        Time accumulator;
        std::vector<Time> data;
        std::vector<Time>::size_type index;

        SampleData()
            : accumulator()
            , data()
            , index()
        {}

        void swap(SampleData& other) {
            std::swap(accumulator, other.accumulator);
            std::swap(data, other.data);
            std::swap(index, index);
        }
    } sample;

    Range<Time, Time> time;
    Range<float, std::uint64_t> freq;

    Clock clock;
};




