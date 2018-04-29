#pragma once 

#include <imgui.h>

class GridLayout
{
    ImGui::ImVec2 screen_size_;
    ImGui::ImVec2 grid_division_;

    int menu_height_ { 20 };

public:
    bool active { true };

    GridLayout( 
        const ImGui::ImVec2& screen_size,
        ImGui::ImVec2 grid_division = ImVec2( 10, 10 ),
        int menu_height = 20 );

    void placeNextWindow( 
        const ImGui::ImVec2& corner1, 
        const ImGui::ImVec2& corner2, 
        const char* mode ,
        int border = 1 ) const;
    
    void resize( const ImGui::ImVec2& screen_size ); 
};