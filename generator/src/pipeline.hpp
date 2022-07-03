#pragma once

namespace generator::pipeline {
  enum class RESULT {
    SUCCESS,
    INVALID_ARGS,
    INVALID_INPUT_DIR,
    UNREADABLE_FILE,
    ERROR_READING_FILE,
    MML_PARSE_ERROR,
  };

  RESULT run_pipeline(int argc, const char** argv);
}
