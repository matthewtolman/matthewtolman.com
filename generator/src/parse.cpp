#include "parse.hpp"

#include <stack>

#include "str_utils.h"
#include <variant_visit.h>

using namespace str_utils;
using namespace generator::parse::mml;

enum class TOKEN_TYPE {
  CONTENT,
  TAG
};

enum class ALLOWED_TAGS {
  ALL,
  BRACE_ONLY
};

static std::optional<Element> parse_element(std::string::iterator& iter, const std::string::iterator& end, ALLOWED_TAGS allowedTags = ALLOWED_TAGS::ALL);

static std::optional<std::vector<std::variant<Content, Tag>>> parse_elements(std::string::iterator& start, const std::string::iterator end, ALLOWED_TAGS allowedTags = ALLOWED_TAGS::ALL) {
  auto       iter = start;

  std::vector<Element> res;
  while(auto parsed = parse_element(iter, end, allowedTags)) {
    res.emplace_back(*parsed);
  }

  if (iter != end) {
    return std::nullopt;
  }
  start = iter;
  return res;
}

static std::optional<Element> parse_content(std::string::iterator& iter, const std::string::iterator& end) {
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

static std::optional<Element> parse_tag(
    std::string::iterator& iter,
    const std::string::iterator& end,
    ALLOWED_TAGS allowedTags = ALLOWED_TAGS::ALL
) {
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

  auto res = Tag{
    Tag::TYPE::EOL,
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
    auto contentEnd = find_not_escaped_stack(segStart, end, '}', '{');
    if (contentEnd == end) {
      return std::nullopt;
    }
    iter = contentEnd + 1;
    res.type = Tag::TYPE::BRACE;
    res.origText = range_to_view(currentTextStart, iter);
    res.rawContent = range_to_view(contentStart, contentEnd);
    res.content = parse_elements(contentStart, contentEnd, ALLOWED_TAGS::BRACE_ONLY);
    return res;
  }
  else if (allowedTags == ALLOWED_TAGS::ALL) {
    auto contentStart = std::find(segStart, end, '\n');
    if (contentStart == end || contentStart + 1 == end) {
      return std::nullopt;
    }
    res.type = Tag::TYPE::BLOCK;
    ++contentStart;

    auto endDelim = std::string(tagName);
    auto propDelim = res.props.find("delim");
    if (propDelim != res.props.end()) {
      if (!propDelim->second.empty()) {
        endDelim = propDelim->second[0];
      }
    }

    res.content = std::vector<Element>{};
    auto nextTagSpot = str_utils::find_after_newline_ws(contentStart, end, '~');
    auto lastTagSpot = contentStart;
    auto substr = std::string("~") + std::string(endDelim) + "~";
    while(nextTagSpot != end) {
      auto elems = parse_elements(lastTagSpot, nextTagSpot - 1);
      if (elems) {
        res.content->insert(res.content->end(), elems->begin(), elems->end());
      }
      else {
        return std::nullopt;
      }
      if (str_utils::starts_with_trails_newline_ws(nextTagSpot, end, substr)) {
        iter           = nextTagSpot + substr.size();
        res.origText   = range_to_view(currentTextStart, iter);
        res.rawContent = range_to_view(contentStart, nextTagSpot);
        return res;
      }
      else {
        auto tagEnd = nextTagSpot;
        auto tag = parse_tag(tagEnd, end);
        if (tagEnd == end || !tag.has_value()) {
          return std::nullopt;
        }
        res.content->emplace_back(*tag);
        lastTagSpot = tagEnd;
        nextTagSpot = str_utils::find_after_newline_ws(tagEnd, end, '~');
      }
    }
    return std::nullopt;
  }

  return std::nullopt;
}

static std::optional<Element> parse_element(std::string::iterator& iter, const std::string::iterator& end, ALLOWED_TAGS allowedTags) {
  auto elem = parse_content(iter, end);
  if (elem.has_value()) {
    return elem;
  }
  return parse_tag(iter, end, allowedTags);
}

std::variant<Document, ParseError> generator::parse::mml::parse(std::shared_ptr<std::string> text) {
  if (!text) {
    return ParseError{ParseError::ERR_CODE::NULL_INPUT};
  }

  Document res = {text, {}};
  if (text->empty()) {
    return res;
  }

  auto       iter = text->begin();
  const auto end  = text->end();
  auto allowedTags = ALLOWED_TAGS::ALL;

  while(auto parsed = parse_element(iter, end, allowedTags)) {
    allowedTags = std::visit(visitor::overload{
        [](const Content& content) { return str_utils::ends_with_newline_ws(content.content) ? ALLOWED_TAGS::ALL : ALLOWED_TAGS::BRACE_ONLY; },
        [](const Tag& tag) { return str_utils::ends_with_newline_ws(tag.origText) || tag.type == Tag::TYPE::BLOCK ? ALLOWED_TAGS::ALL : ALLOWED_TAGS::BRACE_ONLY; }
    }, *parsed);
    res.elements.emplace_back(*parsed);
  }

  if (iter != end) {
    return ParseError{ParseError::ERR_CODE::UNEXPECTED_CHARACTER};
  }

  return res;
}

std::variant<Document, ParseError> generator::parse::mml::parse(const std::string& text) {
  return parse(std::make_shared<std::string>(text));
}

std::variant<Document, ParseError> generator::parse::mml::parse(const char* text) {
  return parse(std::make_shared<std::string>(text));
}
