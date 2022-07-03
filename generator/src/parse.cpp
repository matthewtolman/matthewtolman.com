#include "parse.hpp"
#include "str_utils.h"

using namespace generator::str_utils;

enum class TOKEN_TYPE {
  CONTENT,
  TAG
};

static std::optional<generator::parse::mml::Element> parse_element(std::string::iterator& iter, const std::string::iterator& end);

static std::optional<std::vector<std::variant<generator::parse::mml::Content, generator::parse::mml::Tag>>> parse_elements(std::string::iterator& start, const std::string::iterator end) {
  auto       iter = start;

  std::vector<generator::parse::mml::Element> res;
  while(auto parsed = parse_element(iter, end)) {
    res.emplace_back(*parsed);
  }

  if (iter != end) {
    return std::nullopt;
  }
  start = iter;
  return res;
}

static std::optional<generator::parse::mml::Element> parse_content(std::string::iterator& iter, const std::string::iterator& end) {
  using namespace generator::parse::mml;

  if (iter == end) {
    return std::nullopt;
  }

  const auto curTokenIterStart = iter;
  auto contentEnd        = find_not_escaped(iter, end, '~');

  if (iter == contentEnd) {
    return std::nullopt;
  }

  iter = contentEnd;
  return Content{range_to_view(curTokenIterStart, contentEnd)};
}

static bool is_tag_name_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_prop_name_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static std::optional<std::string::iterator> grab_tag_name(const std::string::iterator& iter, const std::string::iterator& end) {
  auto nameEnd = std::find_if_not(iter, end, is_tag_name_char);
  if (nameEnd == end) {
    return std::nullopt;
  }
  return nameEnd;
}

static std::optional<std::map<std::string, std::vector<std::string_view>>> parse_props(const std::string::iterator& start, const std::string::iterator& end) {
  auto iter = start;
  std::map<std::string, std::vector<std::string_view>> res;
  while (iter != end) {
    auto nameEnd = std::find_if_not(iter, end, is_prop_name_char);
    if (nameEnd == iter || nameEnd == end || *nameEnd != '=') {
      return std::nullopt;
    }
    auto name = std::string(iter, nameEnd);
    auto valueStart = nameEnd + 1;
    decltype(valueStart) valueEnd;
    std::string_view value;

    if (*valueStart == '"') {
      valueEnd = find_not_escaped(valueStart + 1, end, '"');
      if (valueEnd == end) {
        return std::nullopt;
      }
      value = range_to_view(valueStart + 1, valueEnd);
      if (valueEnd + 1 != end){
        if (*(valueEnd + 1) != ';') {
          return std::nullopt;
        }
        else {
          ++valueEnd;
        }
      }
    }
    else {
      valueEnd = find_not_escaped(valueStart, end, ';');
      if (valueEnd == valueStart) {
        return std::nullopt;
      }
      value = range_to_view(valueStart, valueEnd);
    }

    iter = valueEnd == end ? valueEnd : valueEnd + 1;
    res[name].emplace_back(value);
  }
  return res;
}

static std::optional<generator::parse::mml::Element> parse_tag(std::string::iterator& iter, const std::string::iterator& end) {
  if (iter == end) {
    return std::nullopt;
  }

  if (*iter != '~') {
    return std::nullopt;
  }

  auto currentTextStart = iter;
  auto tagNameStart = currentTextStart + 1;
  auto tagNameEndOpt = grab_tag_name(tagNameStart, end);
  if (!tagNameEndOpt) {
    return std::nullopt;
  }
  auto tagName = range_to_view(tagNameStart, *tagNameEndOpt);

  auto res = generator::parse::mml::Tag{
    range_to_view(currentTextStart, iter),
    tagName,
    {},
    std::nullopt
  };

  auto segStart = *tagNameEndOpt;
  if (*segStart == '~') {
    iter = segStart + 1;
    res.origText = range_to_view(currentTextStart, iter);
    return res;
  }

  if (*segStart == '[') {
    auto propStart = segStart + 1;
    auto propEnd = find_not_quoted(segStart, end, ']');
    if (propEnd == end) {
      return std::nullopt;
    }

    segStart = propEnd+1;

    auto props = parse_props(propStart, propEnd);
    if (props.has_value()) {
      res.props = std::move(*props);
    }
    else {
      return std::nullopt;
    }
  }

  if (*segStart == '~') {
    iter = segStart + 1;
    res.origText = range_to_view(currentTextStart, iter);
    return res;
  }
  else if (*segStart == '{') {
    auto contentStart = segStart + 1;
    auto contentEnd = find_not_escaped(contentStart, end, '}');
    if (contentEnd == end) {
      return std::nullopt;
    }
    iter = contentEnd + 1;
    res.origText = range_to_view(currentTextStart, iter);
    res.rawContent = range_to_view(contentStart, contentEnd);
    res.content = parse_elements(contentStart, contentEnd);
    return res;
  }
  else if (*segStart == ' ') {
    auto contentStart = segStart + 1;
    auto contentEnd = std::find(contentStart, end, '\n');
    iter = contentEnd;
    res.origText = range_to_view(currentTextStart, iter);
    res.rawContent = range_to_view(contentStart, contentEnd);
    res.content = parse_elements(contentStart, contentEnd);
    return res;
  }
  else {
    auto contentStart = std::find(segStart, end, '\n');
    if (contentStart == end || contentStart + 1 == end) {
      return std::nullopt;
    }
    ++contentStart;

    auto endDelim = std::string(tagName);
    auto propDelim = res.props.find("delim");
    if (propDelim != res.props.end()) {
      if (!propDelim->second.empty()) {
        endDelim = propDelim->second[0];
      }
    }

    auto substr = std::string("\n~") + std::string(endDelim) + "~";
    auto contentEnd = find_substr_start(contentStart, end, substr);
    if (contentEnd == end) {
      return std::nullopt;
    }

    auto tagEnd = (contentEnd + substr.size());
    iter = tagEnd == end ? tagEnd : tagEnd + 1;
    res.origText = range_to_view(currentTextStart, iter);
    res.rawContent = range_to_view(contentStart, contentEnd);
    res.content = parse_elements(contentStart, contentEnd);
    return res;
  }

  return std::nullopt;
}

static std::optional<generator::parse::mml::Element> parse_element(std::string::iterator& iter, const std::string::iterator& end) {
  auto elem = parse_content(iter, end);
  if (elem.has_value()) {
    return elem;
  }
  return parse_tag(iter, end);
}

std::variant<generator::parse::mml::Document, generator::parse::mml::ParseError> generator::parse::mml::parse(std::shared_ptr<std::string> text) {
  if (!text) {
    return ParseError{ParseError::ERR_CODE::NULL_INPUT};
  }

  Document res = {text, {}};
  if (text->empty()) {
    return res;
  }

  auto       iter = text->begin();
  const auto end  = text->end();

  while(auto parsed = parse_element(iter, end)) {
    res.elements.emplace_back(*parsed);
  }

  if (iter != end) {
    return ParseError{ParseError::ERR_CODE::UNEXPECTED_CHARACTER};
  }

  return res;
}

std::variant<generator::parse::mml::Document, generator::parse::mml::ParseError> generator::parse::mml::parse(const std::string& text) {
  return parse(std::make_shared<std::string>(text));
}

std::variant<generator::parse::mml::Document, generator::parse::mml::ParseError> generator::parse::mml::parse(const char* text) {
  return parse(std::make_shared<std::string>(text));
}
