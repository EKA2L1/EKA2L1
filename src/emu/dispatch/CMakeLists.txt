
add_library(epocdispatch
        include/dispatch/libraries/sysutils/functions.h
        include/dispatch/libraries/sysutils/register.h
        include/dispatch/libraries/register.h
        include/dispatch/audio.h
        include/dispatch/def.h
        include/dispatch/dispatcher.h
        include/dispatch/management.h
        include/dispatch/register.h
        include/dispatch/screen.h
        src/libraries/sysutils/functions.cpp
        src/libraries/register.cpp
        src/audio.cpp
        src/dispatcher.cpp
        src/register.cpp
        src/screen.cpp)

target_include_directories(epocdispatch PUBLIC include)

target_link_libraries(epocdispatch PUBLIC
        bridge
        common)

target_link_libraries(epocdispatch PRIVATE
        drivers
        epoc
        epockern
        epocservs)