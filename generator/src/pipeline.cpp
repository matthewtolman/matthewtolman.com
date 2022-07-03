#include "pipeline.hpp"
#include "cli_args.hpp"
#include "files.h"
#include "parse.hpp"

#include <filesystem>
#include <string>
#include <memory>

generator::pipeline::RESULT generator::pipeline::run_pipeline(int argc, const char **argv) {
  auto cliArgs = generator::cli::parse(argc, argv);
  if (!cliArgs.has_value()) {
    return RESULT::INVALID_ARGS;
  }

  std::vector<std::filesystem::path> loadableFiles;
  try {
    loadableFiles = generator::files::get_loadable_files(cliArgs->inputDirectory);
  } catch (...) {
    return RESULT::INVALID_INPUT_DIR;
  }

  for(const auto& filePath : loadableFiles) {
    std::shared_ptr<std::string> contents = nullptr;

    {
      auto buffer = std::vector<char>{};
      buffer.resize(std::filesystem::file_size(filePath) + 1);
      auto *file = fopen(filePath.c_str(), "r");
      if (file == nullptr) {
        return RESULT::UNREADABLE_FILE;
      }
      try {
        fread(buffer.data(), 1, buffer.size() - 1, file);
        buffer[ buffer.size() - 1 ] = '\0';
      }
      catch (...) {
        fclose(file);
        return RESULT::ERROR_READING_FILE;
      }
      fclose(file);
      contents = std::make_shared<std::string>(buffer.data());
    }

    auto parsedResult = generator::parse::mml::parse(contents);
    if (std::holds_alternative<generator::parse::mml::ParseError>(parsedResult)) {
      return RESULT::MML_PARSE_ERROR;
    }
    auto doc = std::get<generator::parse::mml::Document>(parsedResult);

  }

  return RESULT::SUCCESS;
}
