#pragma once

#include <filesystem>
#include <string>

namespace plazmic {

[[nodiscard]] std::string sha256_file(const std::filesystem::path& path);

}  // namespace plazmic
