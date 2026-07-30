#pragma once

#include <chrono>
#include <string>

#include <sys/types.h>

namespace plazmic {

struct OverlayOptions {
    pid_t target_pid{};
    std::string profile;
    std::chrono::seconds duration{0};
};

[[nodiscard]] int run_x11_overlay(const OverlayOptions& options);

}  // namespace plazmic
