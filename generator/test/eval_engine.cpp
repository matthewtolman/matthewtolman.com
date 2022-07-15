#include "doctest.h"
#include "eval_engine.hpp"

using namespace generator::eval;

TEST_SUITE("Eval Engine") {
  TEST_CASE("Can Parse") {
    auto ctx = Context{};
    SUBCASE("Simple Statement") {
      using ExpectedType = std::vector<Value>;
      auto res = ctx.parse("(+  2 5)");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto resVal = std::get<Context::ParseResult>(res)[0];
      REQUIRE(resVal.has_value());

      auto v = resVal.get_throw<ExpectedType>();

      REQUIRE_EQ(v.size(), 3);

      REQUIRE(v[0].is<Symbol>());
      REQUIRE_EQ(v[0].get_throw<Symbol>().token, "+");

      REQUIRE(v[1].is<double>());
      REQUIRE_EQ(v[1].get_throw<double>(), 2);

      REQUIRE(v[2].is<double>());
      REQUIRE_EQ(v[2].get_throw<double>(), 5);
    }

    SUBCASE("Strings") {
      using ExpectedType = std::string;
      auto res = ctx.parse(R"STR("test String"
"test \t tab" "test \r\n newline" "test \" escape")STR");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto parseRes = std::get<Context::ParseResult>(res);
      auto plain = parseRes[0];
      REQUIRE(plain.has_value());
      auto v = plain.get_throw<ExpectedType>();
      REQUIRE_EQ(v, "test String");

      REQUIRE_EQ(parseRes[1].get_throw<ExpectedType>(), "test \t tab");
      REQUIRE_EQ(parseRes[2].get_throw<ExpectedType>(), "test \n newline");
      REQUIRE_EQ(parseRes[3].get_throw<ExpectedType>(), "test \" escape");
    }

    SUBCASE("Vector") {
      using ExpectedType = std::vector<Value>;
      auto res = ctx.parse("[+ 2 5]");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto resVal = std::get<Context::ParseResult>(res)[0];
      REQUIRE(resVal.has_value());

      auto v = resVal.get_throw<ExpectedType>();

      REQUIRE_EQ(v.size(), 4);

      REQUIRE(v[0].is<Symbol>());
      REQUIRE_EQ(v[0].get_throw<Symbol>().token, "vector");

      REQUIRE(v[1].is<Symbol>());
      REQUIRE_EQ(v[1].get_throw<Symbol>().token, "+");

      REQUIRE(v[2].is<double>());
      REQUIRE_EQ(v[2].get_throw<double>(), 2);

      REQUIRE(v[3].is<double>());
      REQUIRE_EQ(v[3].get_throw<double>(), 5);
    }

    SUBCASE("Map") {
      using ExpectedType = std::map<Value, Value>;
      auto res = ctx.parse("{2 5}");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto resVal = std::get<Context::ParseResult>(res)[0];
      REQUIRE(resVal.has_value());

      auto v = resVal.get_throw<ExpectedType>();

      REQUIRE_EQ(v.size(), 1);

      REQUIRE(v.begin()->first.is<double>());
      REQUIRE_EQ(v.begin()->first.get_throw<double>(), 2);

      REQUIRE(v.begin()->second.is<double>());
      REQUIRE_EQ(v.begin()->second.get_throw<double>(), 5);
    }
  }

  TEST_CASE("Can Run String") {
    SUBCASE("Basic Addition") {
      auto ctx = Context{};
      auto res = ctx.eval("(+ 2 5)");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(7, *res.cast<int>());
    }

    SUBCASE("Write Buffer") {
      auto ctx = Context{};
      auto res = ctx.eval(R"((_core.buf "Hello"))");
      CHECK_EQ("Hello", ctx.str());
    }

    SUBCASE("Write Buffer Twice ") {
      auto ctx = Context{};
      auto res = ctx.eval(R"W((_core.buf (_core.str "Hello " (* 8 5) " ducks"))
(_core.buf "Test"))W");
      CHECK_EQ("Hello 40 ducksTest", ctx.str());
    }
  }
}
