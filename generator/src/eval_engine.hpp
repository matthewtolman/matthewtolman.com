#pragma once

#include <variant_visit.h>

#include <compare>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
#include <list>
#include "str_utils.hpp"

namespace generator::eval {
  struct Error {};

#define EVAL_ENGINE_DEF_OPS(CLASS) \
  bool operator==(const CLASS& other) const { return (*this <=> other) == 0; }; \
  bool operator!=(const CLASS& other) const { return (*this <=> other) != 0; }; \
  bool operator<(const CLASS& other) const { return (*this <=> other) < 0; }; \
  bool operator<=(const CLASS& other) const { return (*this <=> other) <= 0; }; \
  bool operator>(const CLASS& other) const { return (*this <=> other) > 0; }; \
  bool operator>=(const CLASS& other) const { return (*this <=> other) >= 0; };

  struct Symbol {
    std::optional<std::string> ns;
    std::string token;

    auto operator<=>(const Symbol& other) const { return str_utils::bin_compare(token, other.token); }

    EVAL_ENGINE_DEF_OPS(Symbol)
  };

  struct Atom {
    std::string token;

    auto operator<=>(const Atom& other) const { return str_utils::bin_compare(token, other.token); }
    EVAL_ENGINE_DEF_OPS(Atom)
  };

  struct Value;

  struct Frame {
    std::shared_ptr<std::map<std::string, Value>> current = std::make_shared<std::map<std::string, Value>>();
    std::shared_ptr<Frame> parent = nullptr;

    Frame() = default;
    Frame(std::shared_ptr<std::map<std::string, Value>> current, std::shared_ptr<Frame> parent) : current(current), parent(parent) {}
    std::shared_ptr<Frame> add_root_frame(std::shared_ptr<Frame> frame);
    std::strong_ordering operator<=>(const Frame& other) const;
  };

  struct Func {
    std::vector<Symbol>           args;
    std::optional<Symbol>         varArgs;
    std::vector<Value>            statements;
    std::optional<std::string>    desc;
    std::shared_ptr<Frame>        frame;

    std::strong_ordering operator<=>(const Func& other) const;
    EVAL_ENGINE_DEF_OPS(Func)
  };

  struct Macro {
    std::vector<Symbol>           args;
    std::optional<Symbol>         varArgs;
    std::vector<Value>            statements;
    std::optional<std::string>    desc;

    std::strong_ordering operator<=>(const Macro& other) const;
    EVAL_ENGINE_DEF_OPS(Macro)
  };

  struct Value {
    using NativeFunc      = std::function<Value(const std::vector<Value>&)>;
    using NativeFuncTuple = std::tuple<std::string, NativeFunc>;
    std::optional<std::variant<Func, NativeFuncTuple, std::map<Value, Value>, std::list<Value>, std::vector<Value>, std::string, Atom, Symbol, double, bool, Macro>>
        val;

    std::strong_ordering operator<=>(const Value& other) const;
    EVAL_ENGINE_DEF_OPS(Value)

    template<typename T>
    std::optional<T> get() const {
      if (val.has_value() && std::holds_alternative<T>(*val)) {
        return std::get<T>(*val);
      }
      return std::nullopt;
    }

    template<typename T>
    bool is() const {
      return val.has_value() && std::holds_alternative<T>(*val);
    }

    template<typename T>
    T get_throw() const {
      return std::get<T>(*val);
    }

    bool has_value() const { return val.has_value(); }

    std::string to_string() const;
  };

  using NativeFunc = Value::NativeFuncTuple;
}  // namespace generator::eval

std::ostream& operator<<(std::ostream& os, const generator::eval::Value& val);

namespace  generator::eval {
  class Context {
    std::map<std::string, std::map<std::string, Value>> symbols = {};
    std::map<std::string, std::vector<std::string>> fallbackNs = {};
    std::stringstream buf;
    public:
      struct ParseError {
        std::string msg;
      };

      Context();

      using ParseResult = std::vector<Value>;

      std::variant<ParseResult, ParseError> parse(const std::string_view& str);
      Value eval(const std::string_view& str);

      std::string pull_buffer();
      std::string buffer() const;
      std::string current_namespace() const;

    private:
      Value evalValue(const Value& val, std::shared_ptr<Frame> frame);
      Value call(const Value& func, const std::vector<Value>& params, std::shared_ptr<Frame> frame);
      std::shared_ptr<Frame> make_frame(std::shared_ptr<Frame> parent);
      size_t params_to_skip_eval_for(const Value& val);
  };
}  // namespace generator::eval