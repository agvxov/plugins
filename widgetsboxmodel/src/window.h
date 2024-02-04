// Copyright (c) 2022 Manuel Schneider

#pragma once
#include <QPoint>
#include <QWidget>
class ActionDelegate;
class InputLine;
class ItemDelegate;
class QEvent;
class QFrame;
class ResizingList;
class SettingsButton;
class QVBoxLayout;
class QSpacerItem;

class Window : public QWidget
{
public:
    Window();

    QWidget *container;
    QVBoxLayout *window_layout;
    QFrame *frame;
    InputLine *input_line;
    SettingsButton *settings_button;
    ResizingList *results_list;
    ResizingList *actions_list;
    ItemDelegate *item_delegate;
    ActionDelegate *action_delegate;
    QSpacerItem *spacer;

private:
    bool event(QEvent *event) override;

    QPoint clickOffset_;  // The offset from cursor to topleft. Used when the window is dagged
};
