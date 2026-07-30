#include "overlay/x11_overlay.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr const char* kOverlayName = "Plazmic Legends Diagnostics";

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Window find_named_window(Display* display,
                         Window current,
                         const std::string& name) {
    char* current_name = nullptr;
    if (XFetchName(display, current, &current_name) != 0 &&
        current_name != nullptr) {
        const bool matches = name == current_name;
        XFree(current_name);
        if (matches) {
            return current;
        }
    }

    Window root = None;
    Window parent = None;
    Window* children = nullptr;
    unsigned int child_count = 0;
    if (XQueryTree(display, current, &root, &parent, &children, &child_count) ==
        0) {
        return None;
    }
    Window result = None;
    for (unsigned int index = 0; index < child_count && result == None;
         ++index) {
        result = find_named_window(display, children[index], name);
    }
    if (children != nullptr) {
        XFree(children);
    }
    return result;
}

bool wait_for_map_state(Display* display, Window window, int expected) {
    for (int attempt = 0; attempt < 50; ++attempt) {
        XWindowAttributes attributes{};
        if (XGetWindowAttributes(display, window, &attributes) != 0 &&
            attributes.map_state == expected) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

void run_target_child(int ready_fd, int stop_fd) {
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        _exit(EXIT_FAILURE);
    }
    const Window root = DefaultRootWindow(display);
    const Window window =
        XCreateSimpleWindow(display, root, 80, 80, 800, 600, 0, 0, 0x222222);
    XStoreName(display, window, "Plazmic hotkey target");
    const Atom pid_atom = XInternAtom(display, "_NET_WM_PID", False);
    const unsigned long pid = static_cast<unsigned long>(getpid());
    XChangeProperty(display, window, pid_atom, XA_CARDINAL, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&pid), 1);
    XMapWindow(display, window);
    XFlush(display);

    const char ready = '1';
    if (write(ready_fd, &ready, 1) != 1) {
        XCloseDisplay(display);
        _exit(EXIT_FAILURE);
    }
    close(ready_fd);
    char stop = '\0';
    const ssize_t stop_bytes = read(stop_fd, &stop, 1);
    close(stop_fd);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    _exit(stop_bytes == 1 && stop == '1' ? EXIT_SUCCESS : EXIT_FAILURE);
}

int wait_for_child(pid_t child, const std::string& label) {
    int status = 0;
    require(waitpid(child, &status, 0) == child,
            "cannot collect " + label + " child");
    require(WIFEXITED(status), label + " child did not exit normally");
    return WEXITSTATUS(status);
}

}  // namespace

int main() {
    int ready_pipe[2]{};
    int stop_pipe[2]{};
    try {
        require(pipe(ready_pipe) == 0, "cannot create readiness pipe");
        require(pipe(stop_pipe) == 0, "cannot create stop pipe");

        const pid_t target = fork();
        require(target >= 0, "cannot fork target");
        if (target == 0) {
            close(ready_pipe[0]);
            close(stop_pipe[1]);
            run_target_child(ready_pipe[1], stop_pipe[0]);
        }
        close(ready_pipe[1]);
        close(stop_pipe[0]);
        char ready = '\0';
        require(read(ready_pipe[0], &ready, 1) == 1 && ready == '1',
                "target did not become ready");
        close(ready_pipe[0]);

        const pid_t overlay = fork();
        require(overlay >= 0, "cannot fork overlay");
        if (overlay == 0) {
            close(stop_pipe[1]);
            const int status = plazmic::run_x11_overlay({
                .target_pid = target,
                .profile = "hotkey-test",
                .duration = std::chrono::seconds(10),
            });
            _exit(status);
        }

        Display* display = XOpenDisplay(nullptr);
        require(display != nullptr, "cannot open isolated X11 display");
        Window overlay_window = None;
        for (int attempt = 0; attempt < 100 && overlay_window == None;
             ++attempt) {
            overlay_window =
                find_named_window(display, DefaultRootWindow(display),
                                  kOverlayName);
            if (overlay_window == None) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        require(overlay_window != None, "overlay window did not appear");
        require(wait_for_map_state(display, overlay_window, IsViewable),
                "overlay did not become viewable");

        const KeyCode f11 = XKeysymToKeycode(display, XK_F11);
        require(f11 != 0, "isolated display has no F11 keycode");
        require(XTestFakeKeyEvent(display, f11, True, CurrentTime) != 0 &&
                    XTestFakeKeyEvent(display, f11, False, CurrentTime) != 0,
                "cannot inject isolated F11 hide");
        XSync(display, False);
        require(wait_for_map_state(display, overlay_window, IsUnmapped),
                "F11 did not hide overlay");

        require(XTestFakeKeyEvent(display, f11, True, CurrentTime) != 0 &&
                    XTestFakeKeyEvent(display, f11, False, CurrentTime) != 0,
                "cannot inject isolated F11 show");
        XSync(display, False);
        require(wait_for_map_state(display, overlay_window, IsViewable),
                "F11 did not restore overlay");
        XCloseDisplay(display);

        const char stop = '1';
        require(write(stop_pipe[1], &stop, 1) == 1,
                "cannot stop target");
        close(stop_pipe[1]);
        require(wait_for_child(target, "target") == EXIT_SUCCESS,
                "target child failed");
        require(wait_for_child(overlay, "overlay") == EXIT_SUCCESS,
                "overlay child failed");
        std::cout << "isolated F11 hide/show and target-exit cleanup passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
