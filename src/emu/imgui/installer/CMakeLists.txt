add_library(imgui_sis_installer 
        installer.h
        installer.cpp)

target_link_libraries(imgui_sis_installer 
	PRIVATE
        imgui
	imgui_igs
	coremanager)
