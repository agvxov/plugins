// Copyright (c) 2022 Manuel Schneider

#include "actiondelegate.h"
#include "albert/logging.h"
#include "inputline.h"
#include "itemdelegate.h"
#include "resizinglist.h"
#include "settingsbutton.h"
#include "window.h"
#include <QBoxLayout>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QSpacerItem>

#include <QLabel>

Window::Window():
    container(new QWidget(this)),
    window_layout(new QVBoxLayout(this)),
    frame(new QFrame(container)),
    input_line(new InputLine(frame)),
    settings_button(new SettingsButton(container)),
    results_list(new ResizingList(frame)),
    actions_list(new ResizingList(frame)),
    item_delegate(new ItemDelegate(results_list)),
    action_delegate(new ActionDelegate(actions_list)),
    spacer(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed))
{
    results_list->setItemDelegate(item_delegate);
    actions_list->setItemDelegate(action_delegate);

    window_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    window_layout->addItem(spacer);
    window_layout->addWidget(container);

    auto *container_layout = new QHBoxLayout(container);
    container_layout->addWidget(frame);

    auto *frame_layout = new QVBoxLayout(frame);
    frame_layout->addWidget(input_line,0); //, 0, Qt::AlignTop);
    frame_layout->addWidget(results_list,0); //, 0, Qt::AlignTop);
    frame_layout->addWidget(actions_list,0); //, 1, Qt::AlignTop);

    // Identifiers for stylesheets
    frame->setObjectName("frame");
    settings_button->setObjectName("settingsButton");
    input_line->setObjectName("inputLine");
    results_list->setObjectName("resultsList");
    actions_list->setObjectName("actionList");

    window_layout->setContentsMargins(0,0,0,0);
    container_layout->setContentsMargins(0,0,0,0);
    frame_layout->setContentsMargins(0,0,0,0);

    frame->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
    input_line->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
    results_list->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
    actions_list->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);

    settings_button->setFocusPolicy(Qt::NoFocus);
    results_list->setFocusPolicy(Qt::NoFocus);
    actions_list->setFocusPolicy(Qt::NoFocus);
    actions_list->setEditTriggers(QAbstractItemView::NoEditTriggers);

    setWindowFlags(Qt::Tool|Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
}

bool Window::event(QEvent *event)
{
    if (event->type() == QEvent::Close){  // Never close
        hide();
        return true;
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        hide();
        return true;
    }

    else if (event->type() == QEvent::Move) {
        auto *moveEvent = static_cast<QMoveEvent*>(event);
        DEBG << "moveEvent" << moveEvent->oldPos() << ">" << moveEvent->pos();
    }

    else if (event->type() == QEvent::Resize)  // Let settingsbutton be in top right corner of frame
        settings_button->move(frame->geometry().topRight() - QPoint(settings_button->width()-1,0));

    else if (event->type() == QEvent::MouseMove && !clickOffset_.isNull())
        this->move(static_cast<QMouseEvent*>(event)->globalPosition().toPoint() - clickOffset_);

    else if (event->type() == QEvent::MouseButtonPress)
        clickOffset_ = static_cast<QMouseEvent*>(event)->pos();

    else if (event->type() == QEvent::MouseButtonRelease)
        clickOffset_ = QPoint();  // isNull

    return QWidget::event(event);

//    else if (event->type() == QEvent::ApplicationStateChange) {
//        if (hideOnFocusLoss_){
//            auto* app_state = static_cast<QApplicationStateChangeEvent*>(event);
//            if (app_state->applicationState() == Qt::ApplicationInactive)
//                setVisible(false);
//        }
//    }
}
