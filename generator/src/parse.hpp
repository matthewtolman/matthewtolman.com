#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <map>
#include <variant>
#include <vector>

namespace generator::parse {
  namespace mml {
    struct Content {
      std::string_view content;
    };

    struct Tag {
      std::string_view origText;
      std::string_view tagName;
      std::map<std::string, std::vector<std::string_view>> props;
      std::optional<std::vector<std::variant<Content, Tag>>> content;
      std::optional<std::string_view> rawContent;
    };

    using Element = std::variant<Content, Tag>;

    struct Document {
      std::shared_ptr<std::string> origText;
      std::vector<Element> elements;
    };

    struct ParseError {
      enum class ERR_CODE {
        NULL_INPUT,
        UNEXPECTED_CHARACTER
      } code;
    };

    std::variant<Document, ParseError> parse(std::shared_ptr<std::string> text);
    std::variant<Document, ParseError> parse(const std::string& text);
    std::variant<Document, ParseError> parse(const char* text);
  }
}