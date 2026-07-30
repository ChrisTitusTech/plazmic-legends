#include "integration/process_discovery.h"
#include "overlay/x11_overlay.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void run_target_child(int ready_fd) {
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        _exit(77);
    }

    const Window root = DefaultRootWindow(display);
    const Window window =
        XCreateSimpleWindow(display, root, 100, 100, 800, 600, 0, 0, 0x222222);
    XStoreName(display, window, "Plazmic X11 lifecycle target");
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
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    _exit(EXIT_SUCCESS);
}

void run_lifecycle_cycle() {
    int ready_pipe[2]{};
    require(pipe(ready_pipe) == 0, "cannot create readiness pipe");
    const pid_t child = fork();
    require(child >= 0, "cannot fork X11 target");
    if (child == 0) {
        close(ready_pipe[0]);
        run_target_child(ready_pipe[1]);
    }

    close(ready_pipe[1]);
    char ready = '\0';
    const ssize_t ready_bytes = read(ready_pipe[0], &ready, 1);
    close(ready_pipe[0]);
    if (ready_bytes != 1 || ready != '1') {
        int child_status = 0;
        waitpid(child, &child_status, 0);
        if (WIFEXITED(child_status) && WEXITSTATUS(child_status) == 77) {
            throw std::runtime_error("SKIP_NO_X11");
        }
        throw std::runtime_error("X11 target did not become ready");
    }

    const auto started = std::chrono::steady_clock::now();
    const int overlay_status = plazmic::run_x11_overlay({
        .target_pid = child,
        .profile = "lifecycle-test",
        .duration = std::chrono::seconds(5),
    });
    const auto elapsed = std::chrono::steady_clock::now() - started;

    int child_status = 0;
    require(waitpid(child, &child_status, 0) == child,
            "cannot collect X11 target");
    require(WIFEXITED(child_status) &&
                WEXITSTATUS(child_status) == EXIT_SUCCESS,
            "X11 target exited unsuccessfully");
    require(overlay_status == 0, "overlay did not shut down cleanly");
    require(elapsed < std::chrono::seconds(3),
            "overlay did not notice target exit promptly");
    require(!plazmic::is_process_alive(child),
            "target process unexpectedly remains alive");
}

}  // namespace

int main() {
    if (std::getenv("DISPLAY") == nullptr) {
        std::cout << "X11 lifecycle test skipped: DISPLAY is unset\n";
        return 77;
    }
    try {
        for (int cycle = 0; cycle < 3; ++cycle) {
            run_lifecycle_cycle();
        }
        std::cout << "X11 overlay lifecycle passed for three target exits\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        if (std::string(error.what()) == "SKIP_NO_X11") {
            std::cout << "X11 lifecycle test skipped: cannot open display\n";
            return 77;
        }
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
