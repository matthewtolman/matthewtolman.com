#include "doctest.h"
#include "eval_engine.hpp"

using namespace generator::eval;

TEST_SUITE("Eval Engine") {
  TEST_CASE("Can Parse") {
    auto ctx = Context{};
    SUBCASE("Simple Statement") {
      using ExpectedType = std::list<Value>;
      auto res = ctx.parse("(+  2 5)");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto resVal = std::get<Context::ParseResult>(res)[0];
      REQUIRE(resVal.has_value());

      auto v = resVal.get_throw<ExpectedType>();

      REQUIRE_EQ(v.size(), 3);

      auto iter = v.begin();
      REQUIRE(iter->is<Symbol>());
      REQUIRE_EQ(iter->get_throw<Symbol>().token, "+");

      ++iter;
      REQUIRE(iter->is<double>());
      REQUIRE_EQ(iter->get_throw<double>(), 2);

      ++iter;
      REQUIRE(iter->is<double>());
      REQUIRE_EQ(iter->get_throw<double>(), 5);
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

    SUBCASE("Atoms") {
      using ExpectedType = Atom;
      auto res = ctx.parse(R"STR(:test-atom)STR");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto parseRes = std::get<Context::ParseResult>(res);
      auto plain = parseRes[0];
      REQUIRE(plain.has_value());
      auto v = plain.get_throw<ExpectedType>();
      REQUIRE_EQ(v.token, "test-atom");
    }

    SUBCASE("Symbols") {
      using ExpectedType = Symbol;
      auto res = ctx.parse(R"STR(test-sym namespaced.sym)STR");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto parseRes = std::get<Context::ParseResult>(res);
      REQUIRE_EQ(parseRes[0].get_throw<ExpectedType>().token, "test-sym");
      REQUIRE_EQ(parseRes[0].get_throw<ExpectedType>().ns.has_value(), false);
      REQUIRE_EQ(parseRes[1].get_throw<ExpectedType>().token, "sym");
      REQUIRE_EQ(*(parseRes[1].get_throw<ExpectedType>().ns), "namespaced");
    }

    SUBCASE("Numbers") {
      using ExpectedType = double;
      auto res = ctx.parse(R"STR(1 2.4)STR");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto parseRes = std::get<Context::ParseResult>(res);
      REQUIRE_EQ(parseRes[0].get_throw<ExpectedType>(), 1);

      REQUIRE_EQ(parseRes[1].get_throw<ExpectedType>(), 2.4);
    }

    SUBCASE("Bools") {
      using ExpectedType = bool;
      auto res = ctx.parse(R"STR(true false)STR");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto parseRes = std::get<Context::ParseResult>(res);
      REQUIRE_EQ(parseRes[0].get_throw<ExpectedType>(), true);

      REQUIRE_EQ(parseRes[1].get_throw<ExpectedType>(), false);
    }

    SUBCASE("Vector") {
      using ExpectedType = std::vector<Value>;
      auto res = ctx.parse("[+ 2 5]");
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
    SUBCASE("Symbols Resolve") {
      auto ctx = Context{};
      auto res = ctx.eval("__native__.add");
      REQUIRE(res.has_value());
      REQUIRE(res.is<NativeFunc>());
      CHECK_EQ(std::get<0>(res.get_throw<NativeFunc>()), "__add");
    }

    SUBCASE("Numbers Resolve") {
      auto ctx = Context{};
      auto res = ctx.eval("4");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(res.get_throw<double>(), 4.0);
    }

    SUBCASE("Strings Resolve") {
      auto ctx = Context{};
      auto res = ctx.eval(R"STR("Test")STR");
      REQUIRE(res.has_value());
      REQUIRE(res.is<std::string>());
      CHECK_EQ(res.get_throw<std::string>(), "Test");
    }

    SUBCASE("Atoms Resolve") {
      auto ctx = Context{};
      auto res = ctx.eval(":test");
      REQUIRE(res.has_value());
      REQUIRE(res.is<Atom>());
      CHECK_EQ(res.get_throw<Atom>(), Atom{"test"});
    }

    SUBCASE("True Resolves") {
      auto ctx = Context{};
      auto res = ctx.eval("true");
      REQUIRE(res.has_value());
      REQUIRE(res.is<bool>());
      CHECK_EQ(res.get_throw<bool>(), true);
    }

    SUBCASE("False Resolves") {
      auto ctx = Context{};
      auto res = ctx.eval("false");
      REQUIRE(res.has_value());
      REQUIRE(res.is<bool>());
      CHECK_EQ(res.get_throw<bool>(), false);
    }

    SUBCASE("Basic Addition") {
      auto ctx = Context{};
      auto res = ctx.eval("(__native__.add 2 5)");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(7, *res.cast<int>());
    }

    SUBCASE("Vectors Resolve") {
      auto ctx = Context{};
      auto res = ctx.eval("[__native__.add 1]");
      REQUIRE(res.has_value());
      REQUIRE(res.is<std::vector<Value>>());
      auto vec = res.get_throw<std::vector<Value>>();
      auto first = vec[0];
      auto second = vec[1];
      CHECK_EQ(std::get<0>(first.get_throw<NativeFunc>()), "__add");
      CHECK_EQ(second.get_throw<double>(), 1);
    }

    SUBCASE("Write Buffer") {
      auto ctx = Context{};
      auto res = ctx.eval(R"((__native__.buf "Hello"))");
      auto buff = ctx.str();
      CHECK_EQ("Hello", buff);
    }

    SUBCASE("Write Buffer Twice ") {
      auto ctx = Context{};
      auto res = ctx.eval(R"W((__native__.buf (__native__.str "Hello " (__native__.add 8 5) " ducks"))
(__native__.buf "Test"))W");
      CHECK_EQ("Hello 13 ducksTest", ctx.str());
    }
  }
}
