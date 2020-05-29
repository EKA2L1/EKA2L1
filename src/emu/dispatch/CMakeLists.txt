
add_library(epocdispatch
        include/dispatch/audio.h
        include/dispatch/def.h
        include/dispatch/dispatcher.h
        include/dispatch/management.h
        include/dispatch/register.h
        include/dispatch/screen.h
        src/audio.cpp
        src/dispatcher.cpp
        src/register.cpp
        src/screen.cpp)

target_include_directories(epocdispatch PUBLIC include)


target_link_libraries(epocdispatch PUBLIC
        common
        hle)

target_link_libraries(epocdispatch PRIVATE
        drivers
        epoc
        epockern
        epocservs)