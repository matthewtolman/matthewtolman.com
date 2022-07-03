#include <iostream>

#include "pipeline.hpp"

int main(int argc, const char** argv) {
  auto result = generator::pipeline::run_pipeline(argc, argv);
  switch(result) {
    case generator::pipeline::RESULT::SUCCESS:
      break;
    case generator::pipeline::RESULT::INVALID_ARGS:
      std::cerr << "Invalid command line arguments" << std::endl;
      std::cout << generator::cli::usage_docs() << "\n";
      break;
  }
  return static_cast<int>(result);
}