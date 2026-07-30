#include "model/status_snapshot.h"

#include <string_view>

namespace plazmic {

std::string_view compatibility_label(CompatibilityState state) {
    switch (state) {
        case CompatibilityState::not_configured:
            return "Not configured";
        case CompatibilityState::client_error:
            return "Client error";
        case CompatibilityState::unsupported:
            return "Unsupported client";
        case CompatibilityState::supported:
            return "Supported";
    }
    return "Unknown";
}

std::string_view process_label(ProcessState state) {
    switch (state) {
        case ProcessState::unavailable:
            return "Unavailable";
        case ProcessState::not_running:
            return "Not running";
        case ProcessState::running:
            return "Running";
        case ProcessState::ambiguous:
            return "Ambiguous";
        case ProcessState::access_error:
            return "Access error";
    }
    return "Unknown";
}

}  // namespace plazmic
