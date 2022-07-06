#pragma once

#include <string>
#include <string_view>
#include <tuple>

namespace str_utils {
  std::string::iterator find_not_escaped(std::string::iterator start, std::string::iterator end, char look, char escape = '\\');
  std::string::iterator find_not_escaped_stack(std::string::iterator start, std::string::iterator end, char look, char pair, char escape = '\\');
  std::string::iterator find_not_quoted(std::string::iterator start, std::string::iterator end, char look);
  std::string::iterator find_after_newline_ws(std::string::iterator start, std::string::iterator end, char look);
  std::string_view range_to_view(std::string::iterator first, std::string::iterator last);
  std::tuple<std::string::iterator, std::string::iterator> find_substr(std::string::iterator start, std::string::iterator end, const std::string& look);
  std::tuple<std::string::iterator, std::string::iterator> find_substr_followed_by_eof_or_newline(std::string::iterator start, std::string::iterator end, const std::string& look);
  bool starts_with(std::string::iterator start, std::string::iterator end, const std::string& look);
  bool starts_with_trails_newline_ws(std::string::iterator start, std::string::iterator end, const std::string& look);
  bool ends_with_newline_ws(const std::string_view& sv);

  inline std::string_view range_to_view(std::tuple<std::string::iterator, std::string::iterator> bounds) { return range_to_view(std::get<0>(bounds), std::get<1>(bounds)); }
  inline std::string::iterator find_substr_start(std::string::iterator start, std::string::iterator end, const std::string& look) { return std::get<0>(find_substr(start, end, look)); }
}
