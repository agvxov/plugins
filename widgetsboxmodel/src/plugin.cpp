// Copyright (c) 2022-2023 Manuel Schneider

#include "albert/albert.h"
#include "albert/extension/frontend/itemroles.h"
#include "albert/extension/frontend/query.h"
#include "albert/extension/queryhandler/standarditem.h"
#include "albert/logging.h"
#include "inputline.h"
#include "plugin.h"
#include "resizinglist.h"
#include "ui_configwidget.h"
#include <QDir>
#include <QGraphicsEffect>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QtStateMachine/QKeyEventTransition>
#include <QtStateMachine/QSignalTransition>
#include <QtStateMachine/QStateMachine>
#include <QStyleFactory>
#include <utility>
ALBERT_LOGGING_CATEGORY("wbm")
using namespace std;
using namespace albert;

QString getObjectTreeString(QObject *obj, int indent = 0) {
    QString result;
    QTextStream stream(&result);

    QString spaces(indent, ' ');
    stream << spaces << obj->metaObject()->className() << " " << obj << "\n";

    // Recursively traverse child objects
    foreach(QObject *child, obj->children()) {
        result += getObjectTreeString(child, indent + 2);
    }

    return result;
}

namespace  {

const uint    DEF_SHADOW_SIZE = 32;  // TODO user
const char*   STATE_WND_POS  = "windowPosition";

const char*   CFG_CENTERED = "showCentered";
const bool    DEF_CENTERED = true;
const char*   CFG_FOLLOW_CURSOR = "followCursor";
const bool    DEF_FOLLOW_CURSOR = true;
const char*   CFG_THEME = "theme";
const char*   DEF_THEME = "Default";
const char*   CFG_THEME_DARK = "dark_theme";
const char*   DEF_THEME_DARK = "Default";
const char*   CFG_HIDE_ON_FOCUS_LOSS = "hideOnFocusLoss";
const bool    DEF_HIDE_ON_FOCUS_LOSS = true;
const char*   CFG_QUIT_ON_CLOSE = "quitOnClose";
const bool    DEF_QUIT_ON_CLOSE = false;
const char*   CFG_CLEAR_ON_HIDE = "clearOnHide";
const bool    DEF_CLEAR_ON_HIDE = false;
const char*   CFG_ALWAYS_ON_TOP = "alwaysOnTop";
const bool    DEF_ALWAYS_ON_TOP = true;
const char*   CFG_FULLSCREEN = "fullscreen";
const bool    DEF_FULLSCREEN = false;
const char*   CFG_SHOW_FALLBACKS = "showFallbacksOnEmpty";
const bool    DEF_SHOW_FALLBACKS = true;
const char*   CFG_HISTORY_SEARCH = "historySearch";
const bool    DEF_HISTORY_SEARCH = true;
const char*   CFG_MAX_RESULTS = "itemCount";
const uint    DEF_MAX_RESULTS = 5;
const char*   CFG_DISPLAY_SCROLLBAR = "displayScrollbar";
const bool    DEF_DISPLAY_SCROLLBAR = false;
const char*   CFG_CLIENT_SHADOW = "clientShadow";
const bool    DEF_CLIENT_SHADOW = true;
const char*   CFG_SYSTEM_SHADOW = "systemShadow";
const bool    DEF_SYSTEM_SHADOW = true;


//constexpr Qt::KeyboardModifier mods_mod[] = {
//    Qt::ShiftModifier,
//    Qt::MetaModifier,
//    Qt::ControlModifier,
//    Qt::AltModifier
//};
constexpr Qt::Key mods_keys[] = {
    Qt::Key_Shift,
    Qt::Key_Meta,
    Qt::Key_Control,
    Qt::Key_Alt
};

struct CondEventTransition : public QEventTransition {
    CondEventTransition(QObject *object, QEvent::Type type, function<bool()> test):
            QEventTransition(object, type), test_(::move(test)){}
    bool eventTest(QEvent *e) override { return QEventTransition::eventTest(e) && test_(); }
    function<bool()> test_;
};

struct CondKeyEventTransition : public QKeyEventTransition {
    CondKeyEventTransition(QObject *object, QEvent::Type type, int key, function<bool()> test):
            QKeyEventTransition(object, type, key), test_(::move(test)){}
    bool eventTest(QEvent *e) override { return QKeyEventTransition::eventTest(e) && test_(); }
    function<bool()> test_;
};

struct CondSignalTransition : public QSignalTransition {
    template <typename Func>
    CondSignalTransition(const typename QtPrivate::FunctionPointer<Func>::Object *sender,
                         Func sig, function<bool()> test):
            QSignalTransition(sender, sig), test_(::move(test)){}
    bool eventTest(QEvent *e) override { return QSignalTransition::eventTest(e) && test_(); }
    function<bool()> test_;
};

static bool haveDarkPalette()
{
    const QPalette defaultPalette;
    return defaultPalette.color(QPalette::WindowText).lightness()
           > defaultPalette.color(QPalette::Window).lightness();
}

}


Plugin::Plugin()
{
    display_delay_timer.setSingleShot(true);
    display_delay_timer.setInterval(100);

    // reproducible UX
    window.setStyle(QStyleFactory::create("Fusion"));

    // Find themes
    QStringList pluginDataPaths = QStandardPaths::locateAll(
            QStandardPaths::AppDataLocation, id(), QStandardPaths::LocateDirectory);
    for (const QString &pluginDataPath : pluginDataPaths)
        for (const auto &file_info : QDir(QString("%1/themes").arg(pluginDataPath)).entryInfoList(QStringList("*.qss"), QDir::Files | QDir::NoSymLinks))
            themes_.emplace(file_info.baseName(), file_info.canonicalFilePath());

    if (themes_.empty())
        throw runtime_error("No theme files found.");

    {
        auto s = settings();
        setShowCentered(s->value(CFG_CENTERED, DEF_CENTERED).toBool());
        setFollowCursor(s->value(CFG_FOLLOW_CURSOR, DEF_FOLLOW_CURSOR).toBool());
        setHideOnFocusLoss(s->value(CFG_HIDE_ON_FOCUS_LOSS, DEF_HIDE_ON_FOCUS_LOSS).toBool());
        setQuitOnClose(s->value(CFG_QUIT_ON_CLOSE, DEF_QUIT_ON_CLOSE).toBool());
        setClearOnHide(s->value(CFG_CLEAR_ON_HIDE, DEF_CLEAR_ON_HIDE).toBool());
        setAlwaysOnTop(s->value(CFG_ALWAYS_ON_TOP, DEF_ALWAYS_ON_TOP).toBool());
        setFullscreen(s->value(CFG_FULLSCREEN, DEF_FULLSCREEN).toBool());
        setHistorySearchEnabled(s->value(CFG_HISTORY_SEARCH, DEF_HISTORY_SEARCH).toBool());
        setShowFallbacksOnEmptyMatches(s->value(CFG_SHOW_FALLBACKS, DEF_SHOW_FALLBACKS).toBool());
        setMaxResults(s->value(CFG_MAX_RESULTS, DEF_MAX_RESULTS).toUInt());
        setDisplayScrollbar(s->value(CFG_DISPLAY_SCROLLBAR, DEF_DISPLAY_SCROLLBAR).toBool());
        setDisplayClientShadow(s->value(CFG_CLIENT_SHADOW, DEF_CLIENT_SHADOW).toBool());
        setDisplaySystemShadow(s->value(CFG_SYSTEM_SHADOW, DEF_SYSTEM_SHADOW).toBool());
        theme_light_ = s->value(CFG_THEME, DEF_THEME).toString();
        theme_dark_ = s->value(CFG_THEME_DARK, DEF_THEME_DARK).toString();
    }

    {
        auto s = state();
        if (!showCentered()
            && s->contains(STATE_WND_POS)
            && s->value(STATE_WND_POS).canConvert(QMetaType(QMetaType::QPoint)))
            window.move(s->value(STATE_WND_POS).toPoint());
    }

    dark_mode_ = haveDarkPalette();
    applyTheme(dark_mode_ ? theme_dark_ : theme_light_);


    init_statemachine();

    connect(window.input_line, &QLineEdit::textEdited, this, [this]()
    {
        history_.resetIterator();
        user_text = window.input_line->text();
    });

    connect(window.input_line, &QLineEdit::textChanged, this, [this](const QString &text)
    {
        if (current_query){
            current_query->cancel();
            disconnect(current_query.get(), &albert::Query::finished, this, &Plugin::queryFinsished);
            disconnect(current_query->matches(), &QAbstractItemModel::rowsInserted, this, &Plugin::resultsReady);
        }
        current_query = query(text);
        connect(current_query.get(), &albert::Query::finished, this, &Plugin::queryFinsished);
        connect(current_query->matches(), &QAbstractItemModel::rowsInserted, this, &Plugin::resultsReady);
        queries_.push_back(current_query);

//        connect(q, &albert::Query::finished, [](){CRIT << "Query::finished";});
//        connect(&q->matches(), &QAbstractItemModel::rowsInserted, [](){CRIT << "QAbstractItemModel::rowsInserted";});
        window.input_line->setInputHint(current_query->string().isEmpty() ? current_query->synopsis() : QString());
        current_query->run();
    });

    //QTimer::singleShot(0, this, [this](){emit window.input_line->textChanged("");});  // fails because engine is not set

    window.results_list->hide();
    window.actions_list->hide();
    window.input_line->installEventFilter(this);
}

void Plugin::init_statemachine()
{
    // States

    auto *s_top = new QState(QState::ParallelStates);

    auto *s_button = new QState(s_top);

    auto *s_button_hidden = new QState(s_button);
    auto *s_button_shown = new QState(s_button);
    s_button->setInitialState(s_button_hidden);

    auto *s_results = new QState(s_top);

    auto *s_results_hidden = new QState(s_results);
    auto *s_results_postpone = new QState(s_results);
    auto *s_results_visible = new QState(QState::ParallelStates, s_results);
    s_results->setInitialState(s_results_hidden);

    auto *s_results_model = new QState(s_results_visible);
    auto *s_results_model_matches = new QState(s_results_model);
    auto *s_results_model_fallbacks = new QState(s_results_model);
    s_results_model->setInitialState(s_results_model_matches);

    auto *s_results_actions = new QState(s_results_visible);
    auto *s_results_actions_hidden = new QState(s_results_actions);
    auto *s_results_actions_visible = new QState(s_results_actions);
    s_results_actions->setInitialState(s_results_actions_hidden);


    // Debug
//    QObject::connect(s_top, &QState::entered, [](){DEBG<<"s_top::enter";});
//    //QObject::connect(s_button, &QState::entered, [](){DEBG<<"s_button::enter";});
//    //QObject::connect(s_button_hidden, &QState::entered, [](){DEBG<<"s_button_hidden::enter";});
//    QObject::connect(s_button_shown, &QState::entered, [](){DEBG<<"s_button_shown::enter";});
//    //QObject::connect(s_results, &QState::entered, [](){DEBG<<"s_results::enter";});
//    QObject::connect(s_results_hidden, &QState::entered, [](){DEBG<<"s_results_hidden::enter";});
//    QObject::connect(s_results_postpone, &QState::entered, [](){DEBG<<"s_results_postpone::enter";});
//    //QObject::connect(s_results_visible, &QState::entered, [](){DEBG<<"s_results_visible::enter";});
//    //QObject::connect(s_results_model, &QState::entered, [](){DEBG<<"s_results_model::enter";});
//    //QObject::connect(s_results_actions, &QState::entered, [](){DEBG<<"s_results_actions::enter";});
//    QObject::connect(s_results_model_matches, &QState::entered, [](){DEBG<<"s_results_model_matches::enter";});
//    QObject::connect(s_results_model_fallbacks, &QState::entered, [](){DEBG<<"s_results_model_fallbacks::enter";});
//    //QObject::connect(s_results_actions_hidden, &QState::entered, [](){DEBG<<"s_results_actions_hidden::enter";});
//    QObject::connect(s_results_actions_visible, &QState::entered, [](){DEBG<<"s_results_actions_visible::enter";});
//
//    QObject::connect(s_top, &QState::exited, [](){DEBG<<"s_top::exit";});
//    //QObject::connect(s_button, &QState::exited, [](){DEBG<<"s_button::exit";});
//    //QObject::connect(s_button_hidden, &QState::exited, [](){DEBG<<"s_button_hidden::exit";});
//    QObject::connect(s_button_shown, &QState::exited, [](){DEBG<<"s_button_shown::exit";});
//    //QObject::connect(s_results, &QState::exited, [](){DEBG<<"s_results::exit";});
//    QObject::connect(s_results_hidden, &QState::exited, [](){DEBG<<"s_results_hidden::exit";});
//    QObject::connect(s_results_postpone, &QState::exited, [](){DEBG<<"s_results_postpone::exit";});
//    //QObject::connect(s_results_visible, &QState::exited, [](){DEBG<<"s_results_visible::exit";});
//    //QObject::connect(s_results_model, &QState::exited, [](){DEBG<<"s_results_model::exit";});
//    //QObject::connect(s_results_actions, &QState::exited, [](){DEBG<<"s_results_actions::exit";});
//    QObject::connect(s_results_model_matches, &QState::exited, [](){DEBG<<"s_results_model_matches::exit";});
//    QObject::connect(s_results_model_fallbacks, &QState::exited, [](){DEBG<<"s_results_model_fallbacks::exit";});
//    //QObject::connect(s_results_actions_hidden, &QState::exited, [](){DEBG<<"s_results_actions_hidden::exit";});
//    QObject::connect(s_results_actions_visible, &QState::exited, [](){DEBG<<"s_results_actions_visible::exit";});

    // Transitions

    auto setTransition = [](QState *src, QState *dst, QAbstractTransition *transition){
        transition->setTargetState(dst);
        src->addTransition(transition);
        return transition;
    };


    setTransition(s_results_visible, s_results_postpone,
                  new QSignalTransition(window.input_line, &QLineEdit::textChanged));


    setTransition(s_results_postpone, s_results_hidden,
                  new QSignalTransition(&display_delay_timer, &QTimer::timeout));

    setTransition(s_results_postpone, s_results_hidden,
                  new CondSignalTransition(this, &Plugin::queryFinsished,
                                           [this](){return !show_fallbacks_on_empty_query
                                                           || current_query->fallbacks()->rowCount() == 0;}));

    setTransition(s_results_postpone, s_results_model_fallbacks,
                  new CondSignalTransition(this, &Plugin::queryFinsished,
                                           [this](){return show_fallbacks_on_empty_query
                                                           && current_query->fallbacks()->rowCount() > 0;}));

    setTransition(s_results_postpone, s_results_model_fallbacks,
                  new CondKeyEventTransition(window.input_line, QEvent::KeyPress, mods_keys[(int)mod_fallback],
                                             [this](){return current_query->fallbacks()->rowCount() > 0;}));

    setTransition(s_results_postpone, s_results_model_matches,
                  new QSignalTransition(this, &Plugin::resultsReady));


    setTransition(s_results_hidden, s_results_model_fallbacks,
                  new CondSignalTransition(this, &Plugin::queryFinsished,
                                           [this](){return show_fallbacks_on_empty_query
                                                           && current_query->fallbacks()->rowCount() > 0;}));

    setTransition(s_results_hidden, s_results_model_fallbacks,
                  new CondKeyEventTransition(window.input_line, QEvent::KeyPress, mods_keys[(int)mod_fallback],
                                             [this](){return current_query->fallbacks()->rowCount() > 0;}));

    setTransition(s_results_hidden, s_results_model_matches,
                  new QSignalTransition(this, &Plugin::resultsReady));


    setTransition(s_results_model_fallbacks, s_results_hidden,
                  new CondKeyEventTransition(window.input_line, QEvent::KeyRelease, mods_keys[(int)mod_fallback],
                                             [this](){return current_query->matches()->rowCount() == 0;}));

    setTransition(s_results_model_fallbacks, s_results_model_matches,
                  new CondKeyEventTransition(window.input_line, QEvent::KeyRelease, mods_keys[(int)mod_fallback],
                                             [this](){return current_query->matches()->rowCount() != 0;}));


    setTransition(s_results_model_matches, s_results_model_fallbacks,
                  new CondKeyEventTransition(window.input_line, QEvent::KeyPress, mods_keys[(int)mod_fallback],
                                             [this](){return current_query->fallbacks()->rowCount() > 0;}));


    setTransition(s_results_actions_hidden, s_results_actions_visible,
                  new QKeyEventTransition(window.input_line, QEvent::KeyPress, mods_keys[(int)mod_actions]));

    setTransition(s_results_actions_visible, s_results_actions_hidden,
                  new QKeyEventTransition(window.input_line, QEvent::KeyRelease, mods_keys[(int)mod_actions]));


    setTransition(s_button_hidden, s_button_shown,
                  new QEventTransition(window.settings_button, QEvent::Type::Enter));

    setTransition(s_button_hidden, s_button_shown,
                  new QSignalTransition(window.input_line, &QLineEdit::textChanged));


    setTransition(s_button_shown, s_button_hidden,
                  new CondSignalTransition(this, &Plugin::queryFinsished,
                                           [this](){return !window.input_line->underMouse();}));

    setTransition(s_button_shown, s_button_hidden,
                  new CondEventTransition(window.settings_button, QEvent::Type::Leave,
                                          [this](){return current_query->isFinished();}));

    // Behavior

    QObject::connect(s_results_hidden, &QState::entered, this, [this](){
        window.results_list->hide();
    });

    QObject::connect(s_results_postpone, &QState::entered, this, [this](){
        display_delay_timer.start();
        window.results_list->setEnabled(false);
    });

    QObject::connect(s_results_postpone, &QState::exited, this, [this](){
        displayed_query = current_query;
        window.results_list->setEnabled(true);
    });

    QObject::connect(s_results_visible, &QState::entered, this, [this](){
        // Eventfilters are processed in reverse order
        window.input_line->removeEventFilter(this);
        window.input_line->installEventFilter(window.results_list);
        window.input_line->installEventFilter(this);
    });

    QObject::connect(s_results_visible, &QState::exited, this, [this](){
        window.input_line->removeEventFilter(window.results_list);
    });

    QObject::connect(s_results_model_matches, &QState::entered, this, [this](){
        auto *m = current_query->matches();
        auto *sm = window.results_list->selectionModel();
        window.results_list->setModel(m);
        delete sm;
        // let selection model currentChanged set input hint
        disconnect(m, &QAbstractItemModel::rowsInserted, this, &Plugin::resultsReady);
        connect(window.results_list->selectionModel(), &QItemSelectionModel::currentChanged, this,
                [this](const QModelIndex &current, const QModelIndex&) {
                    if (window.results_list->currentIndex().isValid())
                        window.input_line->setInputHint(
                                current.data((int) albert::ItemRoles::InputActionRole).toString());
                });
        if (current_query->string().isEmpty()){ // avoid setting completion when synopsis should be shown
            const QSignalBlocker block(window.results_list->selectionModel());
            window.results_list->setCurrentIndex(m->index(0, 0)); // should be okay since this state requires rc>0
        } else
            window.results_list->setCurrentIndex(m->index(0, 0)); // should be okay since this state requires rc>0
        window.results_list->show();
    });

    QObject::connect(s_results_model_fallbacks, &QState::entered, this, [this](){
        auto *m = current_query->fallbacks();
        if (m != window.results_list->model()){ // Needed because fallback model may already be set
            auto *sm = window.results_list->selectionModel();
            window.results_list->setModel(m);
            delete sm;
            window.results_list->setCurrentIndex(m->index(0, 0)); // should be okay since this state requires rc>0
        }
        window.results_list->show();
    });

    QObject::connect(s_results_actions_visible, &QState::entered, this,
                     [this, s_results_model_matches, s_results_model_fallbacks](){
        // if item is selected and has actions display
        if (window.results_list->currentIndex().isValid()){
            auto *sm = window.actions_list->selectionModel();
            auto *om = window.actions_list->model();
            QAbstractItemModel *m;
            if (s_results_model_matches->active())
                m = current_query->matchActions(window.results_list->currentIndex().row());
            else if (s_results_model_fallbacks->active())
                m = current_query->fallbackActions(window.results_list->currentIndex().row());
            else
                qFatal("Logic error in s_results_actions_shown::entered");
            window.actions_list->setModel(m);
            delete sm;
            delete om;
            window.actions_list->setCurrentIndex(m->index(0, 0)); // should be okay since this state requires rc>0
            window.actions_list->show();
            // Eventfilters are processed in reverse order
            window.input_line->installEventFilter(window.actions_list);
        }
    });

    QObject::connect(s_results_actions_visible, &QState::exited, this, [this](){
        window.actions_list->hide();
        window.input_line->removeEventFilter(window.actions_list);
    });

    auto *graphics_effect = new QGraphicsOpacityEffect(window.settings_button);
    window.settings_button->setGraphicsEffect(graphics_effect);  // QWidget takes ownership of effect.
    auto *opacity_animation = new QPropertyAnimation(graphics_effect, "opacity");
    connect(this, &QWidget::destroyed, opacity_animation, &QObject::deleteLater);
    opacity_animation->setDuration(500);
    opacity_animation->setStartValue(0.0);
    opacity_animation->setEndValue(0.9999999999);  // rounding issues hide button
    opacity_animation->setDirection(QAbstractAnimation::Backward);  // is part of state
    opacity_animation->setEasingCurve(QEasingCurve::InOutQuad);

    QObject::connect(s_button_shown, &QState::entered, this, [opacity_animation](){
        opacity_animation->setDirection(QAbstractAnimation::Forward);
        opacity_animation->start();
    });
    QObject::connect(s_button_shown, &QState::exited, this, [opacity_animation](){
        opacity_animation->setDirection(QAbstractAnimation::Backward);
        opacity_animation->start();
    });

    // Behaviour

    auto *machine = new QStateMachine(this);
    machine->addState(s_top);
    machine->setInitialState(s_top);
    machine->start();

    // Activations

    auto activate = [this, s_results_model_matches, s_results_model_fallbacks](uint i, uint a)
    {
        if (s_results_model_matches->active())
            current_query->activateMatch(i, a);
        else if (s_results_model_fallbacks->active())
            current_query->activateFallback(i, a);
        else
            WARN << "Activated action in neither Match nor Fallback state.";

        // dup intended, catch activations and current text
        history_.add(window.input_line->text());

        if (!QApplication::keyboardModifiers().testFlag(Qt::ControlModifier))
            window.hide();
        else
            // run a new query, things may have changed
            emit window.input_line->textChanged(window.input_line->text());
    };

    QObject::connect(window.results_list, &ResizingList::activated,
                     [activate](const auto &index){activate(index.row(), 0);});

    QObject::connect(window.actions_list, &ResizingList::activated, this,
                     [this, activate](const auto &index){activate(window.results_list->currentIndex().row(),
                                                                  index.row());});
}

bool Plugin::eventFilter(QObject*, QEvent *event)
{
    if (event->type() == QEvent::FocusOut && hideOnFocusLoss_)
        setVisible(false);

    else if (event->type() == QEvent::ApplicationPaletteChange)
    {
        DEBG << "QEvent::ApplicationPaletteChange";
        applyTheme((dark_mode_ = haveDarkPalette()) ? theme_dark_ : theme_light_);
        return true;
    }

    else if (event->type() == QEvent::Close && quitOnClose_)
        qApp->quit();

    else if (event->type() == QEvent::Show) {
      auto screen = getScreen();

        window.settings_button->rotation_animation->start();

        // Trigger a new query on show
        emit window.input_line->textChanged(window.input_line->text());

        // Resize based on the users fullscreen preference
        if (fullscreen_)
        {
         window.container->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

            const auto screen_geo = screen->geometry();
            window.setMinimumSize(screen_geo.size());
            window.resize(screen_geo.size());
        }
        else {
         window.container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

         window.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

            window.resize(window.container->sizeHint());
      }

        // If showCentered or off screen (e.g. display disconnected) move into visible area
        if (showCentered_ || !window.screen()) {
            // move window  TODO remove debugging stuff heree
            auto geo = screen->geometry();

            auto primary_width = window.container->width(); //window.frameSize().width();
            auto newX = geo.center().x() - primary_width / 2;
            auto newY = geo.top() + geo.height() / 5;

            DEBG << screen->name() << screen->manufacturer() << screen->model() << screen->devicePixelRatio() << geo;
            DEBG << "primary_width" << primary_width  << "newX" << newX << "newY" << newY;

            if (fullscreen_)
            {
                window.move(0, 0);
            window.spacer->changeSize(0, geo.height() / 5);
                //window.container->move(newX, newY);
            }
            else
            {
                window.move(newX, newY);
            window.spacer->changeSize(0, 0);
                //window.container->move(0, 0);
            }
        }
    }

    else if (event->type() == QEvent::Hide)
    {
        window.settings_button->rotation_animation->stop();

        state()->setValue(STATE_WND_POS, window.pos());

        if (clearOnHide_)
            window.input_line->clear();
        else
            window.input_line->selectAll();

        // dup intended, catch activations and text-on-hide
        history_.add(window.input_line->text());
        history_.resetIterator();
        user_text.clear();


        // clear all obsolete queries
        queries_.remove_if([this](auto &q){return !(q == current_query || q == displayed_query);});
    }

    else if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
//        DEBG << QKeySequence(keyEvent->key()).toString() << keyEvent->modifiers();
        switch (keyEvent->key()) {

            // Toggle insert completion string
            case Qt::Key_Tab:
                if (window.results_list->currentIndex().isValid()){
                    QString completion = window.results_list->model()->data(
                            window.results_list->currentIndex(),
                            static_cast<int>(albert::ItemRoles::InputActionRole)).toString();
                    if (!(completion.isNull() && completion.isEmpty()))
                        window.input_line->setText(completion);
                }
                return true;

            case Qt::Key_Up:{
                // Move up in the history
                if (!window.results_list->currentIndex().isValid()
                    || keyEvent->modifiers().testFlag(Qt::ShiftModifier)
                    || (window.results_list->currentIndex().row() == 0
                        && !keyEvent->isAutoRepeat())) // ... and first row (non repeat)
                {
                    QString next = history_.next(history_search_ ? user_text : "");

                    // Without ClearOnHide the text is already in the input
                    // I.e. the first item in history equals the input text
                    if (next == window.input_line->text())
                        next = history_.next(history_search_ ? user_text : "");

                    window.input_line->setText(next);

                    return true;
                }
                return false;
            }

            case Qt::Key_Down:{
                // Move down in the history
                if (keyEvent->modifiers().testFlag(Qt::ShiftModifier)) {
                    QString prev = history_.prev(history_search_ ? user_text : "");
                    if (!prev.isEmpty())
                        window.input_line->setText(prev);
                    return true;
                }
                break;
            }

            case Qt::Key_P:
            case Qt::Key_K:
                if (keyEvent->modifiers().testFlag(Qt::ControlModifier)){
                    QKeyEvent e(QEvent::KeyPress, Qt::Key_Up, keyEvent->modifiers().setFlag(Qt::ControlModifier, false));
                    QApplication::sendEvent(window.input_line, &e);
                }
                break;

            case Qt::Key_N:
            case Qt::Key_J:
                if (keyEvent->modifiers().testFlag(Qt::ControlModifier)){
                    QKeyEvent e(QEvent::KeyPress, Qt::Key_Down, keyEvent->modifiers().setFlag(Qt::ControlModifier, false));
                    QApplication::sendEvent(window.input_line, &e);
                }
                break;

            case Qt::Key_H:
                if (keyEvent->modifiers().testFlag(Qt::ControlModifier)){
                    QKeyEvent e(QEvent::KeyPress, Qt::Key_Left, keyEvent->modifiers().setFlag(Qt::ControlModifier, false));
                    QApplication::sendEvent(window.input_line, &e);
                }
                break;

            case Qt::Key_L:
                if (keyEvent->modifiers().testFlag(Qt::ControlModifier)){
                    QKeyEvent e(QEvent::KeyPress, Qt::Key_Right, keyEvent->modifiers().setFlag(Qt::ControlModifier, false));
                    QApplication::sendEvent(window.input_line, &e);
                }
                break;

            case Qt::Key_Comma:{
                if (keyEvent->modifiers() == Qt::ControlModifier || keyEvent->modifiers() == Qt::AltModifier){
                    albert::showSettings();
                    setVisible(false);
                    return true;
                }
                break;
            }

            case Qt::Key_Escape:{
                setVisible(false);
                break;
            }
        }
    }
    return false;
}

bool Plugin::isVisible() const
{
    return window.isVisible();
}

void Plugin::setVisible(bool visible)
{
    // You probably dont want to put code here
    // see QEvent::Hide

    window.setVisible(visible);

    if (visible)
    {
#if not defined Q_OS_MACOS // steals focus on macos
        window.raise();
        window.activateWindow();
#endif
    }
}

QWidget* Plugin::buildConfigWidget()
{
    auto *l = new QLabel(tr("Configure the frontend in the 'Window' tab."));
    l->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return l;
}

QWidget* Plugin::createFrontendConfigWidget()
{
    auto widget = new QWidget;
    Ui::ConfigWidget ui;
    ui.setupUi(widget);

    ui.checkBox_center->setChecked(showCentered());
    connect(ui.checkBox_center, &QCheckBox::toggled,
            this, &Plugin::setShowCentered);

    ui.checkBox_followCursor->setChecked(followCursor());
    connect(ui.checkBox_followCursor, &QCheckBox::toggled,
            this, &Plugin::setFollowCursor);

    ui.checkBox_onTop->setChecked(alwaysOnTop());
    connect(ui.checkBox_onTop, &QCheckBox::toggled,
            this, &Plugin::setAlwaysOnTop);

    ui.checkBox_fullscreen->setChecked(fullscreen());
    connect(ui.checkBox_fullscreen, &QCheckBox::toggled,
            this, &Plugin::setFullscreen);

    ui.checkBox_hideOnFocusOut->setChecked(hideOnFocusLoss());
    connect(ui.checkBox_hideOnFocusOut, &QCheckBox::toggled,
            this, &Plugin::setHideOnFocusLoss);

    ui.checkBox_quit_on_close->setChecked(quitOnClose());
    connect(ui.checkBox_quit_on_close, &QCheckBox::toggled,
            this, &Plugin::setQuitOnClose);

    ui.checkBox_clearOnHide->setChecked(clearOnHide());
    connect(ui.checkBox_clearOnHide, &QCheckBox::toggled,
            this, &Plugin::setClearOnHide);

    ui.checkBox_show_fallbacks->setChecked(showFallbacksOnEmptyMatches());
    connect(ui.checkBox_show_fallbacks, &QCheckBox::toggled,
            this, &Plugin::setShowFallbacksOnEmptyMatches);

    ui.checkBox_history_search->setChecked(historySearchEnabled());
    connect(ui.checkBox_history_search, &QCheckBox::toggled,
            this, &Plugin::setHistorySearchEnabled);

    ui.spinBox_results->setValue((int)maxResults());
    connect(ui.spinBox_results, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Plugin::setMaxResults);

    ui.checkBox_scrollbar->setChecked(displayScrollbar());
    connect(ui.checkBox_scrollbar, &QCheckBox::toggled,
            this, &Plugin::setDisplayScrollbar);

    ui.checkBox_client_shadow->setChecked(displayClientShadow());
    connect(ui.checkBox_client_shadow, &QCheckBox::toggled,
            this, &Plugin::setDisplayClientShadow);

    ui.checkBox_system_shadow->setChecked(displaySystemShadow());
    connect(ui.checkBox_system_shadow, &QCheckBox::toggled,
            this, &Plugin::setDisplaySystemShadow);

    for (const auto&[name, path] : themes())
    {
        ui.comboBox_theme_light->addItem(name, path);
        if (name == theme_light_)
            ui.comboBox_theme_light->setCurrentIndex(ui.comboBox_theme_light->count()-1);
    }
    connect(ui.comboBox_theme_light,
            static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this, comboBox_themes=ui.comboBox_theme_light](int i)
            { setLightTheme(comboBox_themes->itemText(i)); });

    for (const auto&[name, path] : themes())
    {
        ui.comboBox_theme_dark->addItem(name, path);
        if (name == theme_dark_)
            ui.comboBox_theme_dark->setCurrentIndex(ui.comboBox_theme_dark->count()-1);
    }
    connect(ui.comboBox_theme_dark,
            static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this, comboBox_themes=ui.comboBox_theme_dark](int i)
            { setDarkTheme(comboBox_themes->itemText(i)); });

    return widget;
}

unsigned long long Plugin::winId() const { return window.winId(); }

QString Plugin::defaultTrigger() const { return "themes "; }

void Plugin::handleTriggerQuery(TriggerQuery *query) const
{
    for (const auto &[name, path] : themes_)
        if (name.startsWith(query->string(), Qt::CaseInsensitive))
            query->add(
                StandardItem::make(
                    QString("theme_%1").arg(name),
                    name,
                    path,
                    {":app_icon"},
                    {
                        {
                            "apply", tr("Apply theme"),
                            [self=const_cast<Plugin*>(this), n=name](){ self->applyTheme(n); }
                        },
                        {
                            "setlight", tr("Use in light mode"),
                            [self=const_cast<Plugin*>(this), n=name](){ self->setLightTheme(n); }
                        },
                        {
                            "setdark", tr("Use in dark mode"),
                            [self=const_cast<Plugin*>(this), n=name](){ self->setDarkTheme(n); }
                        },
                        {
                            "open", tr("Open theme file"),
                            [p=path](){ openUrl("file://"+p); }
                        }
                    }
                    )
                );
}



/*
 *  PROPERTIES
 */

QString Plugin::input() const { return window.input_line->text(); }

void Plugin::setInput(const QString &input) { window.input_line->setText(input); }

const map<QString, QString> &Plugin::themes() const { return themes_; }

void Plugin::applyTheme(const QString& theme)
{
    QFile f(themes_.at(theme));  // should not fail
    if (f.open(QFile::ReadOnly))
    {
        window.setStyleSheet(f.readAll());
        f.close();
    }
    else
    {
        CRIT << "Set theme does not exist.";
        QMessageBox::critical(nullptr, qApp->applicationDisplayName(),
                              tr("Set theme does not exist."));
    }
}

const QString &Plugin::lightTheme() const
{ return theme_light_; }

void Plugin::setLightTheme(const QString &theme)
{
    settings()->setValue(CFG_THEME, theme_light_ = theme);
    if (!dark_mode_)
        applyTheme(theme);
}

const QString &Plugin::darkTheme() const
{ return theme_dark_; }

void Plugin::setDarkTheme(const QString &theme)
{
    settings()->setValue(CFG_THEME_DARK, theme_dark_ = theme);
    if (dark_mode_)
        applyTheme(theme);
}

uint Plugin::maxResults() const { return window.results_list->maxItems(); }

void Plugin::setMaxResults(uint maxItems)
{
    settings()->setValue(CFG_MAX_RESULTS, maxItems);
    window.results_list->setMaxItems(maxItems);
}

bool Plugin::showCentered() const { return showCentered_; }

void Plugin::setShowCentered(bool b)
{
    settings()->setValue(CFG_CENTERED, b);
    showCentered_ = b;
}

bool Plugin::followCursor() const { return followCursor_; }

void Plugin::setFollowCursor(bool b)
{
    settings()->setValue(CFG_FOLLOW_CURSOR, b);
    followCursor_ = b;
}

bool Plugin::hideOnFocusLoss() const { return hideOnFocusLoss_; }

void Plugin::setHideOnFocusLoss(bool b)
{
    settings()->setValue(CFG_HIDE_ON_FOCUS_LOSS, b);
    hideOnFocusLoss_ = b;
}

bool Plugin::quitOnClose() const { return quitOnClose_; }

void Plugin::setQuitOnClose(bool b)
{
    quitOnClose_ = b;
    settings()->setValue(CFG_QUIT_ON_CLOSE, b);
}

bool Plugin::clearOnHide() const { return clearOnHide_; }

void Plugin::setClearOnHide(bool b)
{
    settings()->setValue(CFG_CLEAR_ON_HIDE, b);
    clearOnHide_ = b;
}

bool Plugin::historySearchEnabled() const { return history_search_; }

void Plugin::setHistorySearchEnabled(bool b)
{
    settings()->setValue(CFG_HISTORY_SEARCH, b);
    history_search_ = b;
}

bool Plugin::showFallbacksOnEmptyMatches() const { return show_fallbacks_on_empty_query; }

void Plugin::setShowFallbacksOnEmptyMatches(bool b)
{
    settings()->setValue(CFG_SHOW_FALLBACKS, b);
    show_fallbacks_on_empty_query = b;
}

bool Plugin::alwaysOnTop() const
{
    return window.windowFlags().testFlag(Qt::WindowStaysOnTopHint);
}

bool Plugin::fullscreen() const
{
    return fullscreen_;
}

void Plugin::setAlwaysOnTop(bool alwaysOnTop)
{
    settings()->setValue(CFG_ALWAYS_ON_TOP, alwaysOnTop);
    window.setWindowFlags(window.windowFlags().setFlag(Qt::WindowStaysOnTopHint, alwaysOnTop));
}

void Plugin::setFullscreen(bool b)
{
    settings()->setValue(CFG_FULLSCREEN, b);
    fullscreen_ = b;
   if (fullscreen_)
   {
      const auto screen = getScreen();
      window.spacer->changeSize(0, screen->geometry().height() / 5);
   }
   else
   {
      window.spacer->changeSize(0, 0);
   }
}

bool Plugin::displayScrollbar() const
{
    return window.results_list->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff;
}

void Plugin::setDisplayScrollbar(bool value)
{
    settings()->setValue(CFG_DISPLAY_SCROLLBAR, value);
    window.results_list->setVerticalScrollBarPolicy(
            value ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

bool Plugin::displayClientShadow() const { return window.graphicsEffect() != nullptr; }

void Plugin::setDisplayClientShadow(bool value)
{
    if (window.graphicsEffect() && !value)
        window.setGraphicsEffect(nullptr);

    if (!window.graphicsEffect() && value){
        // Properties
        auto* effect = new QGraphicsDropShadowEffect(this);
        effect->setBlurRadius(DEF_SHADOW_SIZE);
        effect->setColor(QColor(0, 0, 0, 92))  ;
        effect->setXOffset(0.0);
        effect->setYOffset(2.0);
        window.setGraphicsEffect(effect);  // takes ownership
    }
    value
        ? window.setContentsMargins(DEF_SHADOW_SIZE,DEF_SHADOW_SIZE,DEF_SHADOW_SIZE,DEF_SHADOW_SIZE)
        : window.setContentsMargins(0,0,0,0);
    settings()->setValue(CFG_CLIENT_SHADOW, value);
}

bool Plugin::displaySystemShadow() const
{
    return !window.windowFlags().testFlag(Qt::NoDropShadowWindowHint);
}

void Plugin::setDisplaySystemShadow(bool value)
{
    settings()->setValue(CFG_SYSTEM_SHADOW, value);
    window.setWindowFlags(window.windowFlags().setFlag(Qt::NoDropShadowWindowHint, !value));
}

// get frameoffset or something so 5 is not a magic literal
QScreen *Plugin::getScreen() {
    QScreen *screen = nullptr;
    if (followCursor_){
        if (screen = QGuiApplication::screenAt(QCursor::pos()); !screen){
            WARN << "Could not retrieve screen for cursor position. Using primary screen.";
            screen = QGuiApplication::primaryScreen();
        }
    }
    else {
        screen = QGuiApplication::primaryScreen();
   }

   return screen;
}
