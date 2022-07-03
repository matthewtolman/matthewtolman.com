#pragma once

#include <vector>
#include <string>
#include <filesystem>

namespace generator::files {
  std::vector<std::filesystem::path> get_loadable_files(const std::string& baseDir);
}
