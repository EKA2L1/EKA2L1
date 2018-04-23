#include "ImguiGridLayout.hpp" 

using namespace ImGui;

GridLayout::GridLayout( 
    const ImVec2& screen_size,
    ImVec2 grid_division,
    int menu_height )
    : screen_size_( screen_size )
    , grid_division_ ( grid_division )
    , menu_height_ ( menu_height )
{}

void GridLayout::placeNextWindow( 
    const ImVec2& corner1, 
    const ImVec2& corner2, 
    const char* mode ,
    int border ) const
{
    if( !active )
        return;

    if( strlen( mode ) != 4 )
        return;

    int dx = screen_size_.x / grid_division_.x;
    int dy = (screen_size_.y - menu_height_) / grid_division_.y;

    ImVec2 c1, c2;
    c1.x = (mode[0] == 'l') ? corner1.x * dx : screen_size_.x - corner1.x * dx;
    c1.y = (mode[1] == 't') ? corner1.y * dy : screen_size_.y - menu_height_ - corner1.y * dy;   
    
    c2.x = (mode[2] == 'l') ? corner2.x * dx : screen_size_.x - corner2.x * dx;
    c2.y = (mode[3] == 't') ? corner2.y * dy : screen_size_.y - menu_height_ - corner2.y * dy;   

    SetNextWindowPos ( ImVec2( c1.x + border, c1.y + border + menu_height_ ) );
    SetNextWindowSize( ImVec2( c2.x - c1.x - border, c2.y - c1.y - border) ); 
}

void GridLayout::resize( 
	const ImVec2& screen_size )
{
    screen_size_.x = screen_size.x;
    screen_size_.y = screen_size.y;
}
