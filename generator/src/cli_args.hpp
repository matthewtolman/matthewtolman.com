#pragma once

#include <optional>
#include <string>

namespace generator::cli {
  struct ParsedArgs {
    std::string inputDirectory;
    std::string outputDirectory;
  };

  std::string usage_docs();
  std::optional<ParsedArgs> parse(int argc, const char** argv);
}