project(terminal VERSION 5.0)

albert_plugin(
    SOURCE_FILES
        ${PROJECT_NAME}.qrc
        src/plugin.cpp
        src/plugin.h
    TS_FILES
        i18n/${PROJECT_NAME}_de.ts
)
