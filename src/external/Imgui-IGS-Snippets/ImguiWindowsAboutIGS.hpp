#pragma once

#include <imgui.h>

void drawIGSLogo( 
    const ImVec2& pos,
    const double time );

	
/** Show an info about window with animated IGS logo. 
* \param time in secs.
*/
bool aboutWindow(
    const char* title,
    const char* text,
    const ImVec2& size,
    double time );
