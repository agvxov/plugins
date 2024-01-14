if(${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

project(applications_xdg VERSION 9.0)

albert_plugin(
    SOURCE_FILES
        src/configwidget.ui
        src/plugin.cpp
        src/plugin.h
    TS_FILES
        i18n/${PROJECT_NAME}_de.ts
)

endif()
