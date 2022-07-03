#include "files.h"

#include <set>

static bool loadable(const std::filesystem::directory_entry& dirEntry) {
  static const std::set<std::string> supportedExts = {"mml"};
  return !dirEntry.is_directory() &&
         dirEntry.path().has_extension() &&
         supportedExts.find(dirEntry.path().extension().string().substr(1)) != supportedExts.end();
}

std::vector<std::filesystem::path> generator::files::get_loadable_files(const std::string& baseDir) {
  namespace fs = std::filesystem;
  auto iter = fs::recursive_directory_iterator(fs::path(baseDir));

  auto res = std::vector<std::filesystem::path>();
  for(const auto& dirEntry : iter){
    if (loadable(dirEntry)) {
      res.emplace_back(dirEntry.path());
    }
  }

  return res;
}
