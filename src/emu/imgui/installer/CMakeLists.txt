add_library(imgui_sis_installer 
	installer.h)

target_link_libraries(imgui_sis_installer 
	PRIVATE
	imgui_igs
	coremanager)