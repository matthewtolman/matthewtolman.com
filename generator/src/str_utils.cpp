#include "str_utils.hpp"

std::string::iterator str_utils::find_not_escaped(std::string::iterator start, std::string::iterator end, char look, char escape) {
  auto possibleEnd = std::find(start, end, look);
  if (possibleEnd == end) {
    return end;
  }
  if (possibleEnd == start) {
    return start;
  }
  if (*(possibleEnd - 1) != escape) {
    return possibleEnd;
  }
  return find_not_escaped(possibleEnd + 1, end, look);
}

std::string::iterator str_utils::find_not_quoted(std::string::iterator start, std::string::iterator end, char look) {
  auto quoteCount = 0U;
  auto cur = start;
  auto prevCharIsBackslash = 0;
  while(cur != end && ((quoteCount & 1U) != 0 || *cur != look)) {
    quoteCount += 1 * static_cast<int>(*cur == '"') * (1 - prevCharIsBackslash);
    prevCharIsBackslash = static_cast<int>(*cur == '\\');
    ++cur;
  }
  return cur;
}

std::string_view str_utils::range_to_view(std::string::iterator first, std::string::iterator last) {
  return first != last ? std::string_view { first.operator->(), static_cast<size_t>(last - first) } : std::string_view { nullptr, 0 };
}
std::tuple<std::string::iterator, std::string::iterator> str_utils::find_substr(std::string::iterator start,
                                                                                           std::string::iterator end,
                                                                                           const std::string&    look) {
  for(auto curPos = std::find(start, end, look[0]); curPos != end; curPos = std::find(curPos + 1, end, look[0])) {
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
std::tuple<std::string::iterator, std::string::iterator> str_utils::find_substr_followed_by_eof_or_newline(std::string::iterator start,
                                                                                                           std::string::iterator end,
                                                                                                           const std::string&    look) {
  std::string::iterator curMatch;
  std::string::iterator curEnd;

  for(
      std::tie(curMatch, curEnd) = find_substr(start, end, look);
      curMatch != end && (curEnd != end && *curEnd != '\n');
      std::tie(curMatch, curEnd) = find_substr(curMatch + 1, end, look)
  ) {}

  return std::make_tuple(curMatch, curEnd);
}
std::string::iterator str_utils::find_not_escaped_stack(std::string::iterator start, std::string::iterator end, char look, char pair, char escape) {
  size_t count = 0;

  auto curPos = start;
  while(curPos != end) {
    auto nextEnd = find_not_escaped(curPos, end, look);
    if (nextEnd == end) {
      return end;
    }

    for(auto nextStart = find_not_escaped(curPos, nextEnd, pair); nextStart != nextEnd; nextStart = find_not_escaped(nextStart + 1, nextEnd, pair)) {
      ++count;
    }

    if (count <= 1) {
      return nextEnd;
    }
    --count;
    curPos = nextEnd + 1;
  }
  return end;
}

bool str_utils::starts_with(std::string::iterator start, std::string::iterator end, const std::string& look) {
  if (end - start < look.size()) {
    return false;
  }

  auto l = start;
  for(auto r = look.begin(); l != end && r != look.end(); ++l, ++r) {
    if (*l != *r) {
      return false;
    }
  }
  return true;
}

static bool is_whitespace(char ch) {
  return ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ';
}

std::string::iterator str_utils::find_after_newline_ws(std::string::iterator start, std::string::iterator end, char look) {
  auto curPos = start;
  while (curPos != end) {
    auto newlineItr = std::find(curPos, end, '\n');
    if (newlineItr == end) {
      return end;
    }
    auto lookIter = std::find(newlineItr, end, look);
    bool ws = std::all_of(newlineItr, lookIter, is_whitespace);
    if (lookIter != end && ws) {
      return lookIter;
    }
    curPos = newlineItr + 1;
  }
  return end;
}

bool str_utils::starts_with_trails_newline_ws(std::string::iterator start, std::string::iterator end, const std::string& look) {
  if (end - start < look.size()) {
    return false;
  }

  auto l = start;
  for(auto r = look.begin(); l != end && r != look.end(); ++l, ++r) {
    if (*l != *r) {
      return false;
    }
  }
  while (l != end && *l != '\n') {
    if (!is_whitespace(*l)) {
      return false;
    }
    ++l;
  }
  return true;
}

bool str_utils::ends_with_newline_ws(const std::string_view& sv) {
  if (sv.end() == sv.begin()) {
    return false;
  }
  auto iter = sv.end() - 1;
  for(; iter != sv.begin() && is_whitespace(*iter); --iter) {
    if (*iter == '\n') return true;
  }
  return *iter == '\n';
}
std::strong_ordering str_utils::bin_compare(const std::string& left, const std::string& right) {
  for(auto li = left.begin(), ri = right.begin(); li != left.end() && ri != right.end(); ++li, ++ri) {
    if (*li != *ri) {
      return *li <=> *ri;
    }
  }
  return left.size() <=> right.size();
}
