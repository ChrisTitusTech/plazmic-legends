#include "ui/x11_window_class.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace plazmic {

struct X11WindowClassFilter::State {
    State()
        : is_xcb(QGuiApplication::platformName() == "xcb"),
          display(is_xcb ? XOpenDisplay(nullptr) : nullptr) {}

    bool is_xcb;
    Display* display;
};

X11WindowClassFilter::X11WindowClassFilter(QObject* parent)
    : QObject(parent), state_(new State) {}

X11WindowClassFilter::~X11WindowClassFilter() {
    if (state_->display != nullptr) {
        XCloseDisplay(state_->display);
    }
    delete state_;
}

bool X11WindowClassFilter::available() const {
    return state_->is_xcb && state_->display != nullptr;
}

bool X11WindowClassFilter::apply(QWidget* widget) const {
    if (!state_->is_xcb || state_->display == nullptr || widget == nullptr ||
        !widget->isWindow()) {
        return false;
    }
    XClassHint hint{};
    hint.res_name = const_cast<char*>(kX11Instance);
    hint.res_class = const_cast<char*>(kX11Class);
    const auto window = static_cast<Window>(widget->winId());
    if (XSetClassHint(state_->display, window, &hint) == 0) {
        return false;
    }
    XFlush(state_->display);
    return true;
}

bool X11WindowClassFilter::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Show) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget != nullptr && widget->isWindow()) {
            (void)apply(widget);
            const QPointer<QWidget> guarded(widget);
            QTimer::singleShot(0, this, [this, guarded]() {
                if (guarded != nullptr) {
                    (void)apply(guarded);
                }
            });
        }
    }
    return QObject::eventFilter(watched, event);
}

X11WindowClassFilter* install_x11_window_class_filter(
    QApplication& application) {
    auto* filter = new X11WindowClassFilter(&application);
    application.installEventFilter(filter);
    return filter;
}

}  // namespace plazmic
