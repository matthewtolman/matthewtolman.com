#pragma once

#include <string>
#include <string_view>
#include <tuple>

namespace str_utils {
  std::string::iterator find_not_escaped(std::string::iterator start, std::string::iterator end, char look, char escape = '\\');
  std::string::iterator find_not_quoted(std::string::iterator start, std::string::iterator end, char look);
  std::string_view range_to_view(std::string::iterator first, std::string::iterator last);
  std::tuple<std::string::iterator, std::string::iterator> find_substr(std::string::iterator start, std::string::iterator end, const std::string& look);

  inline std::string_view range_to_view(std::tuple<std::string::iterator, std::string::iterator> bounds) { return range_to_view(std::get<0>(bounds), std::get<1>(bounds)); }
  inline std::string::iterator find_substr_start(std::string::iterator start, std::string::iterator end, const std::string& look) { return std::get<0>(find_substr(start, end, look)); }
}
