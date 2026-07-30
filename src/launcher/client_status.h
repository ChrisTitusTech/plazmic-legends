#pragma once

#include "common/client_file_monitor.h"
#include "game/client_profile.h"
#include "model/status_snapshot.h"

#include <filesystem>
#include <optional>
#include <string>

namespace plazmic {

class ClientStatusProbe {
  public:
    explicit ClientStatusProbe(std::filesystem::path client);

    [[nodiscard]] const std::filesystem::path& client() const {
        return client_;
    }
    [[nodiscard]] const std::string& digest() const { return digest_; }
    [[nodiscard]] const ClientProfile* profile() const { return profile_; }
    [[nodiscard]] StatusSnapshot refresh();

  private:
    std::filesystem::path client_;
    std::string digest_;
    const ClientProfile* profile_{nullptr};
    std::optional<ClientFileMonitor> file_monitor_;
    StatusSnapshot identity_status_;
};

}  // namespace plazmic
