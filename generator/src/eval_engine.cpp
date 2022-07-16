#include "eval_engine.hpp"
#include <iostream>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view.hpp>
#include <functional>
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

std::shared_ptr<generator::eval::Frame> generator::eval::Frame::add_root_frame(std::shared_ptr<Frame> frame) {
  auto newFrame = std::make_shared<Frame>(current, nullptr);
  auto newPrevLayer = newFrame;
  for(auto nextLayer = parent; nextLayer != nullptr; nextLayer = nextLayer->parent) {
    auto newLayer = std::make_shared<Frame>(nextLayer->current, nullptr);
    newPrevLayer->parent = newLayer;
    newPrevLayer = newLayer;
  }
  newPrevLayer->parent = frame;
  return newFrame;
}
std::strong_ordering
generator::eval::Value::operator<=>(const generator::eval::Value &other) const {
  if (!val.has_value()) {
    return other.val.has_value() ? std::strong_ordering::less : std::strong_ordering::equal;
  }
  return std::visit(
      visitor::overload{
          [](const NativeFuncTuple& left, const NativeFuncTuple& right) -> std::strong_ordering {
            return str_utils::bin_compare(std::get<0>(left), std::get<0>(right));
          },
          [](const std::map<Value, Value>& left, const std::map<Value, Value>& right) -> std::strong_ordering {
            for (auto lIter = left.begin(), rIter = right.begin(); lIter != left.end() && rIter != right.end(); ++lIter, ++rIter) {
              auto cmp = lIter->first <=> rIter->first;
              if (lIter->first != rIter->first) {
                return lIter->first <=> rIter->first;
              }
              if (lIter->second != rIter->second) {
                return lIter->second <=> rIter->second;
              }
            }
            return left.size() <=> right.size();
          },
          [](const std::vector<Value>& left, const std::vector<Value>& right) -> std::strong_ordering {
            for (auto lIter = left.begin(), rIter = right.begin(); lIter != left.end() && rIter != right.end(); ++lIter, ++rIter) {
              if (*lIter != *rIter) {
                return *lIter <=> *rIter;
              }
            }
            return left.size() <=> right.size();
          },
          [](const std::string& left, const std::string& right) -> std::strong_ordering { return str_utils::bin_compare(left, right); },
          [](const Atom& left, const Atom& right) -> std::strong_ordering { return left <=> right; },
          [](const Symbol& left, const Symbol& right) -> std::strong_ordering { return left <=> right; },
          [](const double& left, const double& right) -> std::strong_ordering {
            if (left < right) {
              return std::strong_ordering::less;
            }
            else if (left > right) {
              return std::strong_ordering::greater;
            }
            return std::strong_ordering::equal;
          },
          [](const int64_t& left, const int64_t& right) -> std::strong_ordering { return left <=> right; },
          [](const bool& left, const bool& right) -> std::strong_ordering { return static_cast<int>(left) <=> static_cast<int>(right); },
          [ & ](const auto& thisVal, const auto& oVal) -> std::strong_ordering { return val->index() <=> other.val->index(); },
      },
      *val,
      *other.val);
}

std::string generator::eval::Value::to_string() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

static double value_to_double(const generator::eval::Value& val) {
  std::optional<double> num = std::nullopt;
  if (val.template is<double>()) {
    num = val.template get<double>();
  }
  else if (val.template is<bool>()) {
    auto b = val.template get_throw<bool>();
    num = b ? 1 : 0;
  }
  if (!num.has_value()) {
    throw std::runtime_error("Cannot cast value '" + val.to_string() + "' to number");
  }
  return num.value();
}

static generator::eval::Value native_str(const std::vector<generator::eval::Value>& v) {
  std::stringstream ss;
  for (const auto& val : v) {
    if (!val.val.has_value()) {
      ss << "";
    }
    else {
      std::visit(
          visitor::overload{
              [&](const std::string& s) {
                ss << s;
              },
              [&](const auto& v) {
                ss << generator::eval::Value{v};
              }
          }, val.val.value()
      );
    }
  }
  return generator::eval::Value{ss.str()};
}

static generator::eval::Value native_add(const std::vector<generator::eval::Value>& v) {
  if (v.empty()) {
    throw std::runtime_error("Expected at least one argument to __native__.add!");
  }
  return {
      ranges::accumulate(
          ranges::views::transform(
              v, [](const auto &val) { return value_to_double(val); }),
              0.0
      )
  };
}

static generator::eval::Value native_mul(const std::vector<generator::eval::Value>& v) {
  if (v.empty()) {
    throw std::runtime_error("Expected at least one argument to __native__.add!");
  }
  return {
      ranges::accumulate(
          ranges::views::transform(
              v, [](const auto &val) { return value_to_double(val); }),
          1.0,
          std::multiplies<>()
          )
  };
}

static generator::eval::Value native_sub(const std::vector<generator::eval::Value>& v) {
  if (v.empty()) {
    throw std::runtime_error("Expected at least one argument to __native__.add!");
  }
  return {
      ranges::accumulate(
          ranges::views::drop_exactly(v, 1) |
          ranges::views::transform([](const auto &val) { return value_to_double(val); }),
          value_to_double(v[0]),
          std::minus<>()
      )
  };
}

static generator::eval::Value native_div(const std::vector<generator::eval::Value>& v) {
  if (v.empty()) {
    throw std::runtime_error("Expected at least one argument to __native__.add!");
  }
  return {
      ranges::accumulate(
          ranges::views::drop_exactly(v, 1) |
              ranges::views::transform([](const auto &val) { return value_to_double(val); }),
          value_to_double(v[0]),
          std::divides<>()
      )
  };
}

static generator::eval::Value invert_sign(const std::vector<generator::eval::Value>& v) {
  if (v.size() != 1) {
    throw std::runtime_error("Expected arity of one argument to __native__.invert-sign!");
  }
  return {-value_to_double(v[0])};
}

static generator::eval::Value truthy(const std::vector<generator::eval::Value>& v) {
  if (v.size() != 1) {
    throw std::runtime_error("Expected arity of one argument to __native__.truthy!");
  }
  auto val = v[0];
  if (!val.has_value()) {
    return {false};
  }

  return {
    std::visit(
        visitor::overload{
              [](bool b) { return b; },
              [](const std::string& s) { return !s.empty(); },
              [](double d) { return d != 0.0; },
              [](const auto&) { return true; }
        },
        val.val.value()
    )
  };
}

generator::eval::Context::Context() {
  symbols["__native__"] = {};
  symbols["__native__"]["buf"] = Value{std::make_tuple<std::string, Value::NativeFunc>("buf", [&](const std::vector<Value>& v) -> Value {
    for (const auto& val : v) {
      buf << std::visit(
          visitor::overload{
              [&](const std::string& s) { return s; },
              [&](const auto& v) { return Value{v}.to_string(); }
          }, val.val.value()
      );
    }
    return Value{std::nullopt};
  })};
  symbols["__native__"]["str"] = Value{std::make_tuple<std::string, Value::NativeFunc>("str", native_str)};
  symbols["__native__"]["def"] = Value{std::make_tuple<std::string, Value::NativeFunc>("_1def", [&](const std::vector<Value>& v) -> Value {
    if (v.size() != 2) {
      throw std::runtime_error("Invalid arity for def! Expected 2 values!");
    }
    if (!v[0].is<Symbol>()) {
      throw std::runtime_error("Must bind to a Symbol!");
    }
    auto sym = v[0].get_throw<Symbol>();
    if (sym.token.starts_with("__native__.")) {
      throw std::runtime_error("Cannot define symbols in native namespace");
    }
    auto ns = sym.ns ? *sym.ns : current_namespace();
    symbols[ns][sym.token] = v[1];
    return Value{std::nullopt};
  })};
  symbols["__native__"]["add"] = Value{std::make_tuple<std::string, Value::NativeFunc>("add", native_add)};
  symbols["__native__"]["sub"] = Value{std::make_tuple<std::string, Value::NativeFunc>("sub", native_sub)};
  symbols["__native__"]["mul"] = Value{std::make_tuple<std::string, Value::NativeFunc>("mul", native_mul)};
  symbols["__native__"]["div"] = Value{std::make_tuple<std::string, Value::NativeFunc>("div", native_div)};
  symbols["__native__"]["invert-sign"] = Value{std::make_tuple<std::string, Value::NativeFunc>("invert-sign", invert_sign)};
  symbols["__native__"]["truthy"] = Value{std::make_tuple<std::string, Value::NativeFunc>("truthy", truthy)};
}

generator::eval::Value generator::eval::Context::eval(const std::string_view &str) {
  auto parsed = parse(str);
  if (std::holds_alternative<ParseError>(parsed)) {
    throw std::runtime_error(std::get<ParseError>(parsed).msg);
  }
  auto parsedVals = std::get<ParseResult>(parsed);
  Value lastRes{std::nullopt};
  for (const auto& v : parsedVals) {
    lastRes = evalValue(v, nullptr);
  }
  return lastRes;
}

std::shared_ptr<generator::eval::Frame> generator::eval::Context::make_frame(std::shared_ptr<generator::eval::Frame> parent) {
  using namespace generator::eval;
  auto frame = std::make_shared<Frame>();
  (*frame->current)["let"] = {std::make_tuple<std::string, Value::NativeFunc>(
      "__frame.let",
      [&](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
          throw std::runtime_error("Must have arguments to 'let'");
        }
        if (!args[0].is<std::vector<Value>>()) {
          throw std::runtime_error("First argument to 'let' must be a vector");
        }
        auto newFrame = make_frame(frame);
        auto argVec = std::get<std::vector<Value>>(*args[0].val);
        for(auto it = argVec.begin(); it != argVec.end(); ++it) {
          auto key = *it;
          if (!key.is<Symbol>()) {
            throw std::runtime_error("'let' can only bind to symbols!");
          }
          ++it;
          if (it == argVec.end()) {
            throw std::runtime_error("Missing value for " + it->to_string());
          }
          auto val = evalValue(*it, newFrame);
          (*newFrame->current)[key.to_string()] = val;
        }
        Value lastRes{std::nullopt};
        for(auto argIt = args.begin() + 1; argIt != args.end(); ++argIt) {
          lastRes = evalValue(*argIt, newFrame);
        }
        return lastRes;
      })};
  return frame;
}

generator::eval::Value generator::eval::Context::evalValue(const Value &val, std::shared_ptr<Frame> frame) {
  if (frame == nullptr) {
    frame = make_frame(frame);
  }
  if (val.val.has_value()) {
    return std::visit(
        visitor::overload{
            [&](const Symbol &sym) -> Value {
              if (!sym.ns) {
                for (auto curFrame = frame; curFrame != nullptr; curFrame = curFrame->parent) {
                  auto it = curFrame->current->find(sym.token);
                  if (it != curFrame->current->end()) {
                    return it->second;
                  }
                }

                {
                  auto it = this->symbols[current_namespace()].find(sym.token);
                  if (it != this->symbols[current_namespace()].end()) {
                    return it->second;
                  }
                }

                auto &fallbackList = this->fallbackNs[current_namespace()];
                for (const auto &fallback : fallbackList) {
                  auto it = this->symbols[fallback].find(sym.token);
                  if (it != this->symbols[fallback].end()) {
                    return it->second;
                  }
                }
                throw std::runtime_error("Could not find symbol " + sym.token);
              } else {
                auto ns = *sym.ns;
                auto it = this->symbols[ns].find(sym.token);
                if (it != this->symbols[ns].end()) {
                  return it->second;
                }
                throw std::runtime_error("Could not find symbol " + ns + "." +
                                         sym.token);
              }
            },
            [&](const std::vector<Value>& vec) -> Value {
              auto res = std::vector<Value>{};
              res.resize(vec.size());
              std::transform(vec.begin(), vec.end(), res.begin(), [&](const auto& v){ return evalValue(v, frame); });
              return {res};
            },
            [&](const std::list<Value>& list) -> Value {
              if (list.empty()) {
                return Value{std::nullopt};
              }
              auto method = evalValue(*list.begin(), frame);
              auto params = std::vector<Value>{};
              params.resize(list.size() - 1);
              auto begin = list.begin();
              ++begin;

              auto toSkip = params_to_skip_eval_for(method);
              std::transform(
                  begin, list.end(), params.begin(),
                  [&](const auto &v) {
                    if (toSkip) {
                      --toSkip;
                      return v;
                    }
                    return evalValue(v, frame);
                  }
              );
              return call(method, params, frame);
            },
            [](const auto &v) -> Value { return {v}; }},
        *val.val);
  }
  return val;
}

generator::eval::Value generator::eval::Context::call(const Value& func, const std::vector<Value>& params, std::shared_ptr<Frame> frame) {
  if (!func.val.has_value()) {
    throw std::runtime_error("Cannot call 'nil'!");
  }
  return std::visit(
      visitor::overload{
          [&](const Func& func) -> Value {
            if (params.size() < func.args.size() || (!func.varArgs.has_value() && func.args.size() != params.size())) {
              throw std::runtime_error("Expected arity " + std::to_string(func.args.size()) + " but received  " + std::to_string(params.size()) + " params.");
            }
            auto funcFrame = make_frame(frame->add_root_frame(func.frame));
            auto paramIt = params.begin();
            for (auto argIt = func.args.begin(); argIt != func.args.end(); ++paramIt, ++argIt) {
              (*funcFrame->current)[Value{*argIt}.to_string()] = *paramIt;
            }

            if (func.varArgs.has_value()) {
              std::vector<Value> v;
              v.reserve(params.end() - paramIt);
              for(; paramIt != params.end(); ++paramIt) {
                v.push_back(*paramIt);
              }
              (*funcFrame->current)[Value{func.varArgs.value()}.to_string()] = Value{v};
            }
            return evalValue(Value{func.statements}, funcFrame);
          },
          [&](const NativeFunc& func) -> Value {
            return std::get<1>(func)(params);
          },
          [&](bool b) -> Value {
            if (b && !params.empty()) {
              return params[0];
            }
            else if (!b && params.size() >= 2) {
              return params[1];
            }
            return Value{std::nullopt};
          },
          [](const auto& nonFunc) -> Value {
            throw std::runtime_error("Invalid callable!");
          }
      },
      func.val.value()
  );
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
      auto val = std::list<Value>{};
      for(auto curToken = std::get<1>(*begin); curToken.type != TOKEN_TYPE::PAREN_END; curToken = std::get<1>(*begin)) {
        EVAL_ENGINE_GET_VALUE_OR_RET_ERR(v)
        val.template emplace_back(v);
      }
      ++begin;
      return Value{val};
    }
    case TOKEN_TYPE::BRACKET_START: {
      auto val = std::vector<Value>{};
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
    case TOKEN_TYPE::SYMBOL: {
      auto pos = token.sv.find_last_of(".");
      if (pos != std::string::npos) {
        auto ns = token.sv.substr(0, pos);
        auto val = token.sv.substr(pos + 1);
        return Value{generator::eval::Symbol{ns, val}};
      }
      return Value{generator::eval::Symbol{std::nullopt, token.sv}};
    }
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
  auto tokenRange = ranges::views::tokenize(str, std::regex{R"re((:?[a-zA-Z_\*\/\$\@\!\?][\.\w\-\+\=\*\/\$\@\!\?]*)|([\(\[\{\}\]\)])|([\-\+]?(\d+(\.\d+)?)?)|("([^\\"]|(\\.))*"))re"})
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

std::string generator::eval::Context::pull_buffer() {
  auto res = buf.str();
  buf.str("");
  return res;
}

std::string generator::eval::Context::buffer() const {
  return buf.str();
}

std::string generator::eval::Context::current_namespace() const {
  return std::string("core");
}

std::strong_ordering generator::eval::Frame::operator<=>(const generator::eval::Frame& other) const {
  if (current == other.current && parent == other.parent) {
    return std::strong_ordering::equal;
  }
  if (!current != !other.current) {
    return current ? std::strong_ordering::greater : std::strong_ordering::less;
  }
  else if (!current) {
    return std::strong_ordering::equal;
  }

  if (!parent != !other.parent) {
    return parent ? std::strong_ordering::greater : std::strong_ordering::less;
  }

  for (auto lIter = current->begin(), rIter = other.current->begin(); lIter != current->end() && rIter != other.current->end(); ++lIter, ++rIter) {
    if (lIter->first != rIter->first) {
      if (lIter->first < rIter->first) {
        return std::strong_ordering::less;
      }
      return std::strong_ordering::greater;;
    }
    if (lIter->second != rIter->second) {
      return lIter->second <=> rIter->second;
    }
  }
  if (current->size() != other.current->size()) {
    return current->size() <=> other.current->size();
  }

  if (parent) {
    return (*parent) <=> (*other.parent);
  }
  else {
    return std::strong_ordering::equal;
  }
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
  comp = compareVec(statements, other.statements);
  if (comp != 0) {
    return comp;
  }
  if (desc.has_value() != other.desc.has_value()) {
    return desc.has_value() ? std::strong_ordering::greater : std::strong_ordering::less;
  }
  if(desc.value() != other.desc.value()) {
    return desc.value() > other.desc.value() ? std::strong_ordering::greater : std::strong_ordering::less;
  }
  if ((frame == nullptr) != (other.frame == nullptr)) {
    return (frame == nullptr) ? std::strong_ordering::greater : std::strong_ordering::less;
  }
  if (frame) {
    return *frame <=> *other.frame;
  }
  return std::strong_ordering::equal;
}


std::strong_ordering generator::eval::Macro::operator<=>(const generator::eval::Macro& other) const {
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
  comp = compareVec(statements, other.statements);
  if (comp != 0) {
    return comp;
  }
  if (desc.has_value() != other.desc.has_value()) {
    return desc.has_value() ? std::strong_ordering::greater : std::strong_ordering::less;
  }
  if(desc.value() != other.desc.value()) {
    return desc.value() > other.desc.value() ? std::strong_ordering::greater : std::strong_ordering::less;
  }
  return std::strong_ordering::equal;
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
            // TODO: encode frame, description
            os << "(__native__.fn ["
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
          [&](const std::list<Value>& vec) {
            os << "("
               << (vec |
                   ranges::views::transform([](const auto& e) {
                     std::stringstream ss;
                     ss << e;
                     return ss.str();
                   }) |
                   ranges::views::join(" ")
                       )
               << ")";
          },
          [&](const Atom& a) {
            os << ":" << a.token;
          },
          [&](const Symbol& s) {
            if (s.ns) {
              os << s.ns.value() << ".";
            }
            os << s.token;
          },
          [&](const Macro& macro) {
            os << "(__native__.macro ["
               << (
                      macro.args |
                      ranges::views::transform([](const Symbol& sym) { return sym.token; }) |
                      ranges::views::join(" ")
                          )
               << (macro.varArgs.has_value() ? "& " + macro.varArgs->token : "")
               << "] ";

            for (const auto& stmt : macro.statements) {
              os << "(" << stmt << ")";
            }
            os << ")";
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

size_t generator::eval::Context::params_to_skip_eval_for(const generator::eval::Value& val) {
  if (!val.val.has_value()) {
    return 0;
  }
  return std::visit(
      visitor::overload{
          [](const NativeFunc& nativeFunc) -> size_t {
            auto name = std::get<0>(nativeFunc);
            if (name.starts_with("__")) {
              return std::numeric_limits<size_t>::max();
            }
            else if (name.starts_with("_")) {
              return static_cast<size_t>(strtoull(name.c_str() + 1, nullptr, 10));
            }
            return 0;
          },
          [](const Macro& value) -> size_t {
            return std::numeric_limits<size_t>::max();
          },
          [](const auto& value) -> size_t {
              return 0;
          }
      },
      val.val.value()
    );
}
