#include "overlay/x11_overlay.h"

#include "integration/process_discovery.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <X11/keysym.h>

namespace plazmic {
namespace {

volatile std::sig_atomic_t stop_requested = 0;
std::atomic_bool x11_error{false};

void request_stop(int signal);
int record_x11_error(Display* display, XErrorEvent* event);

class SignalHandlerGuard {
  public:
    SignalHandlerGuard()
        : previous_interrupt_(std::signal(SIGINT, request_stop)),
          previous_terminate_(std::signal(SIGTERM, request_stop)) {}

    ~SignalHandlerGuard() {
        if (previous_interrupt_ != SIG_ERR) {
            std::signal(SIGINT, previous_interrupt_);
        }
        if (previous_terminate_ != SIG_ERR) {
            std::signal(SIGTERM, previous_terminate_);
        }
    }

    SignalHandlerGuard(const SignalHandlerGuard&) = delete;
    SignalHandlerGuard& operator=(const SignalHandlerGuard&) = delete;

  private:
    using Handler = void (*)(int);
    Handler previous_interrupt_;
    Handler previous_terminate_;
};

class XErrorHandlerGuard {
  public:
    XErrorHandlerGuard() : previous_(XSetErrorHandler(record_x11_error)) {}

    ~XErrorHandlerGuard() { XSetErrorHandler(previous_); }

    XErrorHandlerGuard(const XErrorHandlerGuard&) = delete;
    XErrorHandlerGuard& operator=(const XErrorHandlerGuard&) = delete;

  private:
    XErrorHandler previous_;
};

void request_stop(int /*signal*/) {
    stop_requested = 1;
}

int record_x11_error(Display* /*display*/, XErrorEvent* /*event*/) {
    x11_error.store(true);
    return 0;
}

pid_t window_pid(Display* display, Window window, Atom pid_atom) {
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long item_count = 0;
    unsigned long bytes_after = 0;
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(
        display, window, pid_atom, 0, 1, False, XA_CARDINAL, &actual_type,
        &actual_format, &item_count, &bytes_after, &data);
    pid_t pid = 0;
    if (status == Success && actual_type == XA_CARDINAL &&
        actual_format == 32 && item_count == 1 && data != nullptr) {
        pid = static_cast<pid_t>(*reinterpret_cast<unsigned long*>(data));
    }
    if (data != nullptr) {
        XFree(data);
    }
    return pid;
}

Window find_window_for_pid(Display* display,
                           Window root,
                           Atom pid_atom,
                           pid_t target_pid) {
    if (window_pid(display, root, pid_atom) == target_pid) {
        return root;
    }
    Window returned_root = None;
    Window returned_parent = None;
    Window* children = nullptr;
    unsigned int child_count = 0;
    if (XQueryTree(display, root, &returned_root, &returned_parent, &children,
                   &child_count) == 0) {
        return None;
    }

    Window result = None;
    for (unsigned int index = 0; index < child_count && result == None;
         ++index) {
        if (window_pid(display, children[index], pid_atom) == target_pid) {
            result = children[index];
        } else {
            result =
                find_window_for_pid(display, children[index], pid_atom,
                                    target_pid);
        }
    }
    if (children != nullptr) {
        XFree(children);
    }
    return result;
}

void draw_overlay(Display* display,
                  Window overlay,
                  GC graphics,
                  const std::string& profile) {
    XSetForeground(display, graphics, 0x111111UL);
    XFillRectangle(display, overlay, graphics, 0, 0, 430, 104);
    XSetForeground(display, graphics, 0x8ee6a8UL);
    const std::array<std::string, 4> lines{
        "Plazmic Legends - Phase 1 diagnostics",
        "Profile: " + profile,
        "External read-only proof; no game state",
        "F11 toggles this panel",
    };
    int baseline = 24;
    for (const auto& line : lines) {
        XDrawString(display, overlay, graphics, 14, baseline, line.c_str(),
                    static_cast<int>(line.size()));
        baseline += 23;
    }
}

void grab_toggle_key(Display* display, Window root) {
    const KeyCode key = XKeysymToKeycode(display, XK_F11);
    constexpr std::array<unsigned int, 4> modifiers{
        0U,
        LockMask,
        Mod2Mask,
        LockMask | Mod2Mask,
    };
    for (const unsigned int modifier : modifiers) {
        XGrabKey(display, static_cast<int>(key), modifier, root, False,
                 GrabModeAsync, GrabModeAsync);
    }
}

bool root_coordinates(Display* display,
                      Window target,
                      Window root,
                      int& x,
                      int& y) {
    Window child = None;
    return XTranslateCoordinates(display, target, root, 0, 0, &x, &y,
                                 &child) != 0;
}

}  // namespace

int run_x11_overlay(const OverlayOptions& options) {
    stop_requested = 0;
    x11_error.store(false);
    SignalHandlerGuard signal_handlers;

    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        std::cerr << "overlay error: cannot open X11 display\n";
        return 20;
    }
    XErrorHandlerGuard x_error_handler;
    const Window root = DefaultRootWindow(display);
    const Atom pid_atom = XInternAtom(display, "_NET_WM_PID", False);
    Window target =
        find_window_for_pid(display, root, pid_atom, options.target_pid);
    if (target == None) {
        std::cerr << "overlay error: no X11 window belongs to selected PID\n";
        XCloseDisplay(display);
        return 21;
    }

    XWindowAttributes target_attributes{};
    if (XGetWindowAttributes(display, target, &target_attributes) == 0) {
        std::cerr << "overlay error: cannot inspect target window\n";
        XCloseDisplay(display);
        return 22;
    }
    int target_x = 0;
    int target_y = 0;
    if (!root_coordinates(display, target, root, target_x, target_y)) {
        std::cerr << "overlay error: cannot locate target window\n";
        XCloseDisplay(display);
        return 22;
    }

    XSetWindowAttributes attributes{};
    attributes.override_redirect = True;
    attributes.background_pixel = 0x111111UL;
    attributes.event_mask = ExposureMask | StructureNotifyMask;
    const Window overlay = XCreateWindow(
        display, root, target_x + 24, target_y + 44, 430, 104, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);
    XStoreName(display, overlay, "Plazmic Legends Diagnostics");

    const Atom opacity_atom =
        XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
    const unsigned long opacity = 0xd9999999UL;
    XChangeProperty(display, overlay, opacity_atom, XA_CARDINAL, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&opacity), 1);

    XserverRegion empty_region = XFixesCreateRegion(display, nullptr, 0);
    XFixesSetWindowShapeRegion(display, overlay, ShapeInput, 0, 0,
                               empty_region);
    XFixesDestroyRegion(display, empty_region);
    int rectangle_count = 0;
    int rectangle_order = 0;
    XRectangle* input_rectangles = XShapeGetRectangles(
        display, overlay, ShapeInput, &rectangle_count, &rectangle_order);
    if (input_rectangles != nullptr) {
        XFree(input_rectangles);
    }
    if (rectangle_count != 0) {
        std::cerr << "overlay error: input region is not empty\n";
        XDestroyWindow(display, overlay);
        XCloseDisplay(display);
        return 23;
    }
    std::cout << "overlay_input_region=empty\n";

    GC graphics = XCreateGC(display, overlay, 0, nullptr);
    XSelectInput(display, root, KeyPressMask);
    XSync(display, False);
    if (x11_error.exchange(false)) {
        std::cerr << "overlay error: X11 setup failed\n";
        XFreeGC(display, graphics);
        XDestroyWindow(display, overlay);
        XCloseDisplay(display);
        return 25;
    }
    grab_toggle_key(display, root);
    XSync(display, False);
    if (x11_error.exchange(false)) {
        std::cerr << "overlay error: F11 is already reserved by another "
                     "X11 client\n";
        XFreeGC(display, graphics);
        XDestroyWindow(display, overlay);
        XCloseDisplay(display);
        return 24;
    }
    bool visible = true;
    bool mapped = target_attributes.map_state == IsViewable;
    if (mapped) {
        XMapRaised(display, overlay);
    }
    XFlush(display);

    const auto started = std::chrono::steady_clock::now();
    while (stop_requested == 0) {
        while (XPending(display) != 0) {
            XEvent event{};
            XNextEvent(display, &event);
            if (event.type == Expose && event.xexpose.window == overlay) {
                draw_overlay(display, overlay, graphics, options.profile);
            } else if (event.type == KeyPress) {
                const KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_F11) {
                    visible = !visible;
                    std::cout << "overlay_visible="
                              << (visible ? "true" : "false") << '\n';
                    if (!visible && mapped) {
                        XUnmapWindow(display, overlay);
                        mapped = false;
                    }
                }
            }
        }

        XWindowAttributes current{};
        if (XGetWindowAttributes(display, target, &current) == 0 ||
            x11_error.exchange(false)) {
            break;
        }
        if (!is_process_alive(options.target_pid)) {
            break;
        }
        if (!root_coordinates(display, target, root, target_x, target_y) ||
            x11_error.exchange(false)) {
            break;
        }
        const bool should_map =
            visible && current.map_state == IsViewable;
        if (should_map && !mapped) {
            XMapRaised(display, overlay);
            mapped = true;
        } else if (!should_map && mapped) {
            XUnmapWindow(display, overlay);
            mapped = false;
        }
        if (should_map) {
            XMoveWindow(display, overlay, target_x + 24, target_y + 44);
            XRaiseWindow(display, overlay);
        }
        if (options.duration.count() > 0 &&
            std::chrono::steady_clock::now() - started >= options.duration) {
            break;
        }
        XFlush(display);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    XUngrabKey(display, AnyKey, AnyModifier, root);
    XFreeGC(display, graphics);
    XDestroyWindow(display, overlay);
    XCloseDisplay(display);
    return 0;
}

}  // namespace plazmic
