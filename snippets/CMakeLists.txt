project(snippets VERSION 5.0)

albert_plugin(
    SOURCE_FILES
        ${PROJECT_NAME}.qrc
        src/configwidget.ui
        src/plugin.cpp
        src/plugin.h
        include/snippets.h
    PUBLIC_INCLUDE_DIRECTORIES
        include
    TS_FILES
        i18n/${PROJECT_NAME}_de.ts
)
