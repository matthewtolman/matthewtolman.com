#include "eval_engine.hpp"
#include <iostream>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/view.hpp>
#include <magic_enum.hpp>
#include <stack>

static bool is_whitespace(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == ',';
}

namespace generator::eval::internal {
enum class TOKEN_TYPE {
  PAREN_START,
  PAREN_END,
  BRACKET_START,
  BRACKET_END,
  BRACE_START,
  BRACE_END,
  NUMBER,
  ATOM,
  QUOTE,
  SYMBOL,
  STRING,
  NIL,
  TRUE,
  FALSE,
  INVALID,
};
struct Token {
  TOKEN_TYPE type;
  std::string sv;
};
}

generator::eval::Context::Context() {
  symbols[{"_core.buf"}] = Value{std::make_tuple<std::string, Value::NativeFunc>("core_buf", [&](const std::vector<Value>& v) -> Value {
    for (const auto& val : v) {
      buf << val << "\n";
    }
    return Value{std::nullopt};
  })};
  symbols[{"_core.str"}] = Value{std::make_tuple<std::string, Value::NativeFunc>("core_str", [](const std::vector<Value>& v) -> Value {
    std::stringstream ss;
    for (const auto& val : v) {
      ss << val << "\n";
    }
    return Value{ss.str()};
  })};
}

generator::eval::Value generator::eval::Context::eval(const std::string_view &str) {
  auto parsed = parse(str);
  return Value{std::nullopt};
}

std::string unescape(const std::string& str) {
  static std::regex tabRe{"\\\\t"};
  static std::regex newlineRe{"(\\\\r)?\\\\n"};
  static std::regex miscRe{"\\\\(.)"};
  return
      std::regex_replace(
          std::regex_replace(
            std::regex_replace(str, tabRe, "\t"),
            newlineRe,
            "\n"
          ),
        miscRe,
        "$1"
      );
}

static generator::eval::internal::Token str_to_token(const std::string& s) {
  using namespace generator::eval::internal;
  auto ch = s[0];
  if (ch == '[') {
    return {TOKEN_TYPE::BRACKET_START, s};
  }
  else if (ch == ']') {
    return {TOKEN_TYPE::BRACKET_END, s};
  }
  else if (ch == '(') {
    return {TOKEN_TYPE::PAREN_START, s};
  }
  else if (ch == ')') {
    return {TOKEN_TYPE::PAREN_END, s};
  }
  else if (ch == '{') {
    return {TOKEN_TYPE::BRACE_START, s};
  }
  else if (ch == '}') {
    return {TOKEN_TYPE::BRACE_END, s};
  }
  else if (ch == '\'') {
    return {TOKEN_TYPE::QUOTE, s};
  }
  else if ((ch >= '0' && ch <= '9') ||
           ((ch == '+' || ch == '-') && s.size() > 1)) {
    return {TOKEN_TYPE::NUMBER, s};
  }
  else if (ch == ':') {
    return {TOKEN_TYPE::ATOM, s};
  }
  else if (ch == '"') {
    return {TOKEN_TYPE::STRING, s};
  }
  else if (s == "nil") {
    return {TOKEN_TYPE::NIL, s};
  }
  else if (s == "true") {
    return {TOKEN_TYPE::TRUE, s};
  }
  else if (s == "false") {
    return {TOKEN_TYPE::FALSE, s};
  }
  else {
    return {TOKEN_TYPE::SYMBOL, s};
  }
}

static constexpr bool is_end_token(generator::eval::internal::TOKEN_TYPE t) {
  using TOKEN_TYPE = generator::eval::internal::TOKEN_TYPE;
  return t == TOKEN_TYPE::BRACE_END | t == TOKEN_TYPE::BRACKET_END | t == TOKEN_TYPE::PAREN_END;
}

static constexpr bool is_start_token(generator::eval::internal::TOKEN_TYPE t) {
  using TOKEN_TYPE = generator::eval::internal::TOKEN_TYPE;
  return t == TOKEN_TYPE::BRACE_START | t == TOKEN_TYPE::BRACKET_START | t == TOKEN_TYPE::PAREN_START;
}

static constexpr generator::eval::internal::TOKEN_TYPE get_expected_end(generator::eval::internal::TOKEN_TYPE t) {
  using TOKEN_TYPE = generator::eval::internal::TOKEN_TYPE;
  switch(t) {
  case TOKEN_TYPE::BRACKET_START:
    return TOKEN_TYPE::BRACKET_END;
  case TOKEN_TYPE::BRACE_START:
    return TOKEN_TYPE::BRACE_END;
  case TOKEN_TYPE::PAREN_START:
    return TOKEN_TYPE::PAREN_END;
  default:
    return TOKEN_TYPE::INVALID;
  }
}

#define EVAL_ENGINE_GET_VALUE_OR_RET_ERR(VAR_NAME) \
  auto tmp_ ## VAR_NAME = getValue(begin, end); \
  if (std::holds_alternative<generator::eval::Context::ParseError>(tmp_ ## VAR_NAME)) { \
    return std::get<generator::eval::Context::ParseError>(tmp_ ## VAR_NAME);                       \
  }                                                \
  auto VAR_NAME = std::get<generator::eval::Value>(tmp_ ## VAR_NAME);

template<typename Iter, typename IterEnd>
static std::variant<generator::eval::Value, generator::eval::Context::ParseError> getValue(Iter& begin, const IterEnd& end) {
  using TOKEN_TYPE = generator::eval::internal::TOKEN_TYPE;
  using Value = generator::eval::Value;
  auto token = std::get<1>(*begin);
  ++begin;

  switch (token.type) {
    case TOKEN_TYPE::PAREN_START: {
      auto val = std::vector<Value>{};
      for(auto curToken = std::get<1>(*begin); curToken.type != TOKEN_TYPE::PAREN_END; curToken = std::get<1>(*begin)) {
        EVAL_ENGINE_GET_VALUE_OR_RET_ERR(v)
        val.template emplace_back(v);
      }
      ++begin;
      return Value{val};
    }
    case TOKEN_TYPE::BRACKET_START: {
      auto val = std::vector<Value>{};
      val.emplace_back(Value{generator::eval::Symbol{"vector"}});
      for(auto curToken = std::get<1>(*begin); curToken.type != TOKEN_TYPE::BRACKET_END; curToken = std::get<1>(*begin)) {
        EVAL_ENGINE_GET_VALUE_OR_RET_ERR(v)
        val.template emplace_back(v);
      }
      ++begin;
      return Value{val};
    }
    case TOKEN_TYPE::BRACE_START:{
      auto map = std::map<Value, Value>{};
      for(auto curToken = std::get<1>(*begin); curToken.type != TOKEN_TYPE::BRACE_END; curToken = std::get<1>(*begin)) {
        EVAL_ENGINE_GET_VALUE_OR_RET_ERR(key)
        curToken = std::get<1>(*begin);
        if (curToken.type == TOKEN_TYPE::BRACE_END) {
          return generator::eval::Context::ParseError{"Missing value in map"};
        }
        EVAL_ENGINE_GET_VALUE_OR_RET_ERR(val)
        map[key] = val;
      }
      ++begin;
      return Value{map};
    }
    case TOKEN_TYPE::NUMBER:
      return Value{strtod(token.sv.c_str(), nullptr)};
    case TOKEN_TYPE::ATOM:
      return Value{generator::eval::Atom{token.sv.substr(1)}};
    case TOKEN_TYPE::QUOTE:
      return Value{generator::eval::Quote{}};
    case TOKEN_TYPE::SYMBOL:
      return Value{generator::eval::Symbol{token.sv}};
    case TOKEN_TYPE::STRING:
      return Value{unescape(token.sv.substr(1, token.sv.size() - 2))};
    case TOKEN_TYPE::NIL:
      return Value{std::nullopt};
    case TOKEN_TYPE::TRUE:
      return Value{true};
    case TOKEN_TYPE::FALSE:
      return Value{false};
  }
  return generator::eval::Context::ParseError{"Invalid Token"};
}

std::variant<std::vector<generator::eval::Value>, generator::eval::Context::ParseError> generator::eval::Context::parse(const std::string_view &str) {
  auto tokenRange = ranges::views::tokenize(str, std::regex{R"re(([a-zA-Z_\*\/\$\@\!\?][\.\w\-\+\=\*\/\$\@\!\?]*)|([\(\[\{'\}\]\)])|([\-\+]?(\d+(\.\d+)?)?)|("([^\\"]|(\\.))*"))re"})
     | ranges::views::transform([](const auto& s) { return s.str(); })
     | ranges::views::filter([](const auto& s) { return !s.empty(); })
     | ranges::views::transform(str_to_token);

  auto tokenIter = ranges::views::enumerate(tokenRange);

  std::stack<internal::TOKEN_TYPE> expected;
  for(const auto& v : tokenIter) {
    auto token = std::get<1>(v);
    if (is_start_token(token.type)) {
      expected.push(get_expected_end(token.type));
    }
    else if (is_end_token(token.type)) {
      if (expected.empty() || expected.top() != token.type) {
        return ParseError {std::string("Unexpected token ") + std::string(magic_enum::enum_name(token.type))};
      }
      expected.pop();
    }
  }

  std::vector<Value> res;
  auto begin = tokenIter.begin();
  auto end = tokenIter.end();
  while(begin != end) {
    EVAL_ENGINE_GET_VALUE_OR_RET_ERR(v);
    res.emplace_back(v);
  }

  return res;
}

#undef EVAL_ENGINE_GET_VALUE_OR_RET_ERR

std::string generator::eval::Context::str() {
  auto res = buf.str();
  buf.str("");
  return res;
}

std::string generator::eval::Context::buffer() const {
  return buf.str();
}

std::strong_ordering generator::eval::Func::operator<=>(const generator::eval::Func& other) const {
  auto compareVec = [](const auto& left, const auto& right) -> std::strong_ordering {
    for (auto lIter = left.begin(), rIter = right.begin(); lIter != left.end() && rIter != right.end(); ++lIter, ++rIter) {
      if (*lIter != *rIter) {
        return *lIter <=> *rIter;
      }
    }
    return left.size() <=> right.size();
  };
  auto comp = compareVec(args, other.args);
  if (comp != 0) {
    return comp;
  }
  if (varArgs.has_value() != other.varArgs.has_value()) {
    return varArgs.has_value() ? std::strong_ordering::greater : std::strong_ordering::less;
  }
  return compareVec(statements, other.statements);
}

//generator::eval::Parser::TokenIterator::value_type generator::eval::Parser::TokenIterator::operator*() const {
//  return ((*this) + 1).prev;
//}
//
//generator::eval::Parser::TokenIterator::value_type generator::eval::Parser::TokenIterator::operator->() {
//  return ((*this) + 1).prev;
//}
//
//generator::eval::Parser::TokenIterator& generator::eval::Parser::TokenIterator::operator+=(int steps) {
//  if (steps <= 0) {
//    return *this;
//  }
//
//  auto newBegin = view.begin();
//  while (newBegin != view.end() && is_whitespace(*newBegin)) {
//    newBegin++;
//  }
//  auto expectedEndStack = std::stack<char>{};
//  auto getValue = [&]()-> Value {
//    Value res{};
//    auto expectedEnd = get_expected_close(*newBegin);
//    if (expectedEnd) {
//      expectedEndStack.push(expectedEnd);
//    }
//    return res;
//  };
//
//  for(size_t i = 0; i < steps && newBegin != view.end(); ++i) {
//    prev = consumeToken();
//  }
//
//  view = std::string_view{newBegin, view.end() - newBegin};
//  return *this;
//}
//
//bool generator::eval::operator==(
//    const generator::eval::Parser::TokenIterator &a,
//    const generator::eval::Parser::TokenIterator &b) {
//  return (a + 1).view == (b + 1).view;
//}

std::ostream &operator<<(std::ostream &os, const generator::eval::Value &val) {
  using namespace generator::eval;
  if (!val.val.has_value()) {
    os << "nil";
  }
  std::visit(
      visitor::overload{
          [&](const Value::NativeFuncTuple& tuple) {
            os << "<NativeFunc:" << std::get<0>(tuple) << ">";
          },
          [&](const Func& func) {
            os << "(fn ["
               << (
                   func.args |
                   ranges::views::transform([](const Symbol& sym) { return sym.token; }) |
                   ranges::views::join(" ")
               )
               << (func.varArgs.has_value() ? "& " + func.varArgs->token : "")
               << "] ";

            for (const auto& stmt : func.statements) {
              os << "(" << stmt << ")";
            }
            os << ")";
          },
          [&](const std::map<Value, Value>& map) {
            os << "{"
               << (map |
                   ranges::views::transform([](const auto& pair) {
                     std::stringstream ss;
                     ss << pair.first << " " << pair.second;
                     return ss.str();
                   }) |
                   ranges::views::join(" ")
               )
               << "}";
          },
          [&](const std::vector<Value>& vec) {
            os << "["
               << (vec |
                   ranges::views::transform([](const auto& e) {
                     std::stringstream ss;
                     ss << e;
                     return ss.str();
                   }) |
                   ranges::views::join(" ")
               )
               << "]";
          },
          [&](const Atom& a) {
            os << ":" << a.token;
          },
          [&](const Symbol& s) {
            os << s.token;
          },
          [&](const Quote& s) {
            os << "'";
          },
          [&](const std::string& str) {
            os << "\"";
            std::regex_replace(std::ostreambuf_iterator<char>(os), str.begin(), str.end(), std::regex("\""), "\\\"");
            os << "\"";
          },
          [&](bool b) {
            os << (b ? "true" : "false");
          },
          [&](const auto& v) {
            os << v;
          }
      },
      *val.val
  );
  return os;
}
