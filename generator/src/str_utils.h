#pragma once

#include <string>
#include <string_view>
#include <tuple>

namespace generator::str_utils {
  std::string::iterator find_not_escaped(std::string::iterator start, std::string::iterator end, char look);
  std::string::iterator find_not_quoted(std::string::iterator start, std::string::iterator end, char look);
  std::string_view range_to_view(std::string::iterator first, std::string::iterator last);
  std::tuple<std::string::iterator, std::string::iterator> find_substr(std::string::iterator start, std::string::iterator end, const std::string& look);
  std::string::iterator find_substr_start(std::string::iterator start, std::string::iterator end, const std::string& look);
}
