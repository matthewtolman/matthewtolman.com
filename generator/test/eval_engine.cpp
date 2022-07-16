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

    SUBCASE("Nil") {
      using ExpectedType = Atom;
      auto res = ctx.parse(R"STR(nil)STR");
      REQUIRE(std::holds_alternative<Context::ParseResult>(res));
      auto parseRes = std::get<Context::ParseResult>(res);
      auto plain = parseRes[0];
      REQUIRE(!plain.has_value());
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
      CHECK_EQ(std::get<0>(res.get_throw<NativeFunc>()), "add");
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
      CHECK_EQ(7, res.get_throw<double>());
    }

    SUBCASE("Vectors Resolve") {
      auto ctx = Context{};
      auto res = ctx.eval("[__native__.add 1]");
      REQUIRE(res.has_value());
      REQUIRE(res.is<std::vector<Value>>());
      auto vec = res.get_throw<std::vector<Value>>();
      auto first = vec[0];
      auto second = vec[1];
      CHECK_EQ(std::get<0>(first.get_throw<NativeFunc>()), "add");
      CHECK_EQ(second.get_throw<double>(), 1);
    }

    SUBCASE("Write Buffer") {
      auto ctx = Context{};
      auto res = ctx.eval(R"((__native__.buf "Hello"))");
      auto buff = ctx.pull_buffer();
      CHECK_EQ("Hello", buff);
    }

    SUBCASE("Write Buffer Twice ") {
      auto ctx = Context{};
      auto res = ctx.eval(R"W((__native__.buf (__native__.str "Hello " (__native__.add 8 5) " ducks"))
(__native__.buf "Test"))W");
      CHECK_EQ("Hello 13 ducksTest", ctx.pull_buffer());
    }

    SUBCASE("Let Binding") {
      auto ctx = Context{};
      auto res = ctx.eval("(let [a 12 b 4] (__native__.add a b))");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(16, res.get_throw<double>());
    }
  }

  TEST_CASE("Native Methods") {
    SUBCASE("Add") {
      auto ctx = Context{};
      auto res = ctx.eval("(__native__.add 6 8 3 2)");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(19, res.get_throw<double>());
    }

    SUBCASE("Subtract") {
      auto ctx = Context{};
      auto res = ctx.eval("(__native__.sub 16 8 2 1)");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(5, res.get_throw<double>());
    }

    SUBCASE("Multiply") {
      auto ctx = Context{};
      auto res = ctx.eval("(__native__.mul 6 8 3 2)");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(288, res.get_throw<double>());
    }

    SUBCASE("Divide") {
      auto ctx = Context{};
      auto res = ctx.eval("(__native__.div 384 2 3 4)");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(16, res.get_throw<double>());
    }

    SUBCASE("Eval Bool") {
      auto ctx = Context{};
      auto res = ctx.eval("(true 384 2)");
      CHECK_EQ(384, res.get_throw<double>());

      res = ctx.eval("(false 384 2)");
      CHECK_EQ(2, res.get_throw<double>());
    }

    SUBCASE("Truthy") {
      auto ctx = Context{};
      auto res = ctx.eval("(__native__.truthy 384)");
      CHECK_EQ(true, res.get_throw<bool>());

      res = ctx.eval("(__native__.truthy 0)");
      CHECK_EQ(false, res.get_throw<bool>());

      res = ctx.eval("(__native__.truthy \"\")");
      CHECK_EQ(false, res.get_throw<bool>());

      res = ctx.eval("(__native__.truthy \"a\")");
      CHECK_EQ(true, res.get_throw<bool>());

      res = ctx.eval("(__native__.truthy nil)");
      CHECK_EQ(false, res.get_throw<bool>());

      res = ctx.eval("(__native__.truthy :a)");
      CHECK_EQ(true, res.get_throw<bool>());

      res = ctx.eval("(__native__.truthy __native__.add)");
      CHECK_EQ(true, res.get_throw<bool>());
    }

    SUBCASE("Def") {
      auto ctx = Context{};

      // Thest that we can assign
      auto res = ctx.eval("(__native__.def a 23) a");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(23, res.get_throw<double>());

      // Test that we can reassign and use in an add
      res = ctx.eval(R"STR(
(__native__.def a 13)
(__native__.def b 31)
(__native__.add a b))STR");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(44, res.get_throw<double>());

      // Test that we can use in a different eval call (aka. it persists)
      res = ctx.eval(R"STR(
(__native__.sub b a a))STR");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(5, res.get_throw<double>());

      // Test that we can have namespaced definitions
      res = ctx.eval(R"STR(
(__native__.def test.a -13)
(__native__.add a test.a))STR");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(0, res.get_throw<double>());
    }

    SUBCASE("Invert Sign") {
      auto ctx = Context{};
      auto res = ctx.eval("(__native__.invert-sign 23)");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(-23, res.get_throw<double>());

      res = ctx.eval("(__native__.invert-sign -23)");
      REQUIRE(res.has_value());
      REQUIRE(res.is<double>());
      CHECK_EQ(23, res.get_throw<double>());
    }

    SUBCASE("String concatenation") {
      auto ctx = Context{};
      auto res = ctx.eval(R"S((__native__.str 23 "Hello" :hello))S");
      REQUIRE(res.has_value());
      REQUIRE(res.is<std::string>());
      CHECK_EQ("23Hello:hello", res.get_throw<std::string>());
    }

    SUBCASE("Write to buffer") {
      auto ctx = Context{};
      auto res = ctx.eval(R"S((__native__.buf 23 "Hello" :hello))S");
      REQUIRE(!res.has_value());
      CHECK_EQ("23Hello:hello", ctx.pull_buffer());
    }
  }
}
