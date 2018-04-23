#include "ImguiFrameClock.hpp"

namespace
{

void update(detail::FrameClockGuiHelper& data, const FrameClock& clock)
{
    float dt = clock.getLastFrameTime()*1000.0f;
    data.mCurrentRefreshTime += dt;
    if(data.mCurrentRefreshTime > data.mRefreshTime)
    {

        /* FPS Plot */
        data.mPlotFPS[data.mPlotFPSOffset] = clock.getFramesPerSecond();
        data.mPlotFPSOffset = (data.mPlotFPSOffset + 1) % data.mPlotFPS.size();
        data.mCurrentRefreshTime = 0.0f;


        /* FrameTime Histogram */
        unsigned int bin = static_cast<unsigned int>( std::floor(dt) );
        bin = (bin < data.mFrameTimeBins.size()) ? bin : data.mFrameTimeBins.size() - 1;
        data.mFrameTime[bin] += dt;
        data.mFrameTimeBins[bin] = data.mFrameTime[bin];
    }
}

}

void frameClockWindow(const FrameClock &clock)
{
    using namespace ImGui;

    static detail::FrameClockGuiHelper sData;

    update(sData, clock);

    if(!Begin("FrameClock"))
    {
        End();
        return;
    }

    float offsetOne = GetTextLineHeight()*20/4;
    Columns(3, "##fpsColumn", true);
    SetColumnOffset(0, 0);
    SetColumnOffset(1, offsetOne);
    SetColumnOffset(2, GetTextLineHeight()*20/2 + offsetOne);


    Text("Time"); NextColumn();
    Text(detail::stringPrecision(clock.getTotalFrameTime()).c_str()); NextColumn();
    Text("(sec)"); NextColumn();

    Separator();

    Text("Frame"); NextColumn();
    Text(detail::stringPrecision(clock.getTotalFrameCount()).c_str()); NextColumn();
    NextColumn();

    Separator();

    Text("FPS"); NextColumn();
    Text(detail::stringPrecision(clock.getFramesPerSecond()).c_str()); NextColumn();
    NextColumn();

    Text("min."); NextColumn();
    Text(detail::stringPrecision(clock.getMinFramesPerSecond()).c_str()); NextColumn();
    NextColumn();

    Text("avg."); NextColumn();
    Text(detail::stringPrecision(clock.getAverageFramesPerSecond()).c_str()); NextColumn();
    NextColumn();

    Text("max."); NextColumn();
    Text(detail::stringPrecision(clock.getMaxFramesPerSecond()).c_str()); NextColumn();
    NextColumn();

    Separator();

    Text("Delta"); NextColumn();
    Text(detail::stringPrecision(clock.getLastFrameTime()*1000.0f).c_str()); NextColumn();
    Text("(ms)"); NextColumn();

    Text("min."); NextColumn();
    Text(detail::stringPrecision(clock.getMinFrameTime()*1000.0f).c_str()); NextColumn();
    Text("(ms)"); NextColumn();

    Text("avg."); NextColumn();
    Text(detail::stringPrecision(clock.getAverageFrameTime()*1000.0f).c_str()); NextColumn();
    Text("(ms)"); NextColumn();

    Text("max."); NextColumn();
    Text(detail::stringPrecision(clock.getMaxFrameTime()*1000.0f).c_str()); NextColumn();
    Text("(ms)"); NextColumn();

    Columns(1);

    Separator();

    PlotLines("FPS", &sData.mPlotFPS[0], sData.mPlotFPS.size(), sData.mPlotFPSOffset, "", 0.0f, 120.0f, ImVec2(GetTextLineHeight()*17, GetTextLineHeight()*6));

    Separator();

    PlotHistogram("dt", &sData.mFrameTimeBins[0], sData.mFrameTimeBins.size(), 0, "", 0, FLT_MAX, ImVec2(GetTextLineHeight()*17, GetTextLineHeight()*10));

    End();

    return;
}
