#include "str_utils.h"

std::string::iterator generator::str_utils::find_not_escaped(std::string::iterator start, std::string::iterator end, char look) {
  auto possibleEnd = std::find(start, end, look);
  if (possibleEnd == end) {
    return end;
  }
  if (possibleEnd == start) {
    return start;
  }
  if (*(possibleEnd - 1) != '\\') {
    return possibleEnd;
  }
  return find_not_escaped(possibleEnd + 1, end, look);
}

std::string::iterator generator::str_utils::find_not_quoted(std::string::iterator start, std::string::iterator end, char look) {
  auto quoteCount = 0U;
  auto cur = start;
  while(cur != end && ((quoteCount & 1U) != 0 || *cur != look)) {
    quoteCount += 1 * static_cast<int>(*cur == '"');
    ++cur;
  }
  return cur;
}

std::string_view generator::str_utils::range_to_view(std::string::iterator first, std::string::iterator last) {
  return first != last ? std::string_view { first.operator->(), static_cast<size_t>(last - first) } : std::string_view { nullptr, 0 };
}
std::tuple<std::string::iterator, std::string::iterator> generator::str_utils::find_substr(std::string::iterator start,
                                                                                           std::string::iterator end,
                                                                                           const std::string&    look) {
  for(auto curPos = std::find(start, end, look[0]); start != end; curPos = std::find(curPos + 1, end, look[0])) {
    bool found = true;
    auto checkPos = curPos + 1;
    for (auto expectedIter = look.begin() + 1; checkPos != end && expectedIter != look.end() && found; ++checkPos, ++expectedIter) {
      found = *checkPos == *expectedIter;
    }
    if (found) {
      return std::make_tuple(curPos, checkPos);
    }
  }
  return std::make_tuple(end, end);
}
std::string::iterator generator::str_utils::find_substr_start(std::string::iterator start, std::string::iterator end, const std::string& look) {
  auto [substrStart, substrEnd] = find_substr(start, end, look);
  return substrStart;
}
