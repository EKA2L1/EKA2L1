#include "ImguiWindowsAboutIGS.hpp"

#include <cmath>

using namespace ImGui;

void drawIGSLogo( 
    const ImVec2& pos,
    const double time )
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    for( int i = 0; i < 9; ++i )
    {
        int col_val   = (i%2 == 0) ? 0   : 255;

        ImVec2 lpos = pos;
        lpos.x += 4 * i * sin( -time - sin(time - 3.1415/4.0));
        lpos.y += 4 * i * cos( -time - sin(time - 3.1415/4.0));

        int radius = 50 - 5 * i;

        draw_list->AddCircleFilled( lpos, radius, ImColor( col_val, col_val, col_val, 220 ), 30 );
    }
}

bool aboutWindow(
    const char* title,
    const char* text,
    const ImVec2& size,
    double time )
{
    bool close_it = false;
    SetNextWindowSize( size );
    Begin( title );
        SetCursorPosX( GetCursorPosX() + 120 );
        TextWrapped( text );

        Text(" ");
        Text(" "); SameLine( GetWindowWidth() - 60 );

        if( Button("Close") )
           close_it = true;
        
        ImVec2 pos = GetWindowPos();
        pos.x += 55;
        pos.y += 75;

        drawIGSLogo( pos, time );
    End();

    return close_it;    
}