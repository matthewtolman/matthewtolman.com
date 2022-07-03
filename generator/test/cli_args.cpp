#include "cli_args.hpp"

#include <array>

#include "doctest.h"

TEST_SUITE("generator::cli") {
  TEST_CASE("usage_docs") {
    const auto usage = generator::cli::usage_docs();
    CHECK(usage.find("generator <input_directory> <output_directory>") != std::string::npos);
  }

  TEST_CASE("parse") {
    std::array<const char*, 3> args = {"program", "in", "out"};
    CHECK(!generator::cli::parse(1, args.data()).has_value());
    CHECK(!generator::cli::parse(2, args.data()).has_value());
    CHECK(generator::cli::parse(3, args.data()).has_value());

    const auto parsed = generator::cli::parse(3, args.data());
    CHECK_EQ(parsed->inputDirectory, "in");
    CHECK_EQ(parsed->outputDirectory, "out");
  }
}
