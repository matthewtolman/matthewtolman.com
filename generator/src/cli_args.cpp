#include "cli_args.hpp"

#include <vector>

std::string generator::cli::usage_docs() {
  return R"DOCS(
Usage:
  generator <input_directory> <output_directory>

Arguments:
  input_directory   - Directory with input blog data
  output_directory  - Directory for storing the resulting blog files
)DOCS";
}


std::optional<generator::cli::ParsedArgs> generator::cli::parse(int argc, const char **argv) {
  std::vector<std::string> args;
  args.reserve(argc);
  for(int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  if (args.size() < 3) {
    return std::nullopt;
  }

  return ParsedArgs{
      args[1],
      args[2]
  };
}
