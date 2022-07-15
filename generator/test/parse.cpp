#include "parse.hpp"

#include <ostream>

#include "doctest.h"
#include "str_utils.hpp"

std::ostream& operator<<(std::ostream& os, std::shared_ptr<std::string> ptr) {
  if (!ptr) {
    os << "<shared_ptr*:NULL>";
  }
  else {
    os << "<shared_ptr*:\"" << *ptr << "\">";
  }
  return os;
}

TEST_SUITE("generator::parse::mml") {
  TEST_CASE("empty") {
    auto str = std::make_shared<std::string>(R"STR()STR");
    auto res = generator::parse::mml::parse(str);
    CHECK(holds_alternative<generator::parse::mml::Document>(res));
    auto doc = std::get<generator::parse::mml::Document>(res);
    CHECK_EQ(doc.elements.size(), 0);
    CHECK_EQ(doc.origText, str);
  }

  TEST_CASE("parse content only") {
    auto str = std::make_shared<std::string>(R"STR(
Hello World! This
is some test input that is

only content)STR");
    auto res = generator::parse::mml::parse(str);
    CHECK(holds_alternative<generator::parse::mml::Document>(res));
    auto doc = std::get<generator::parse::mml::Document>(res);
    CHECK_EQ(doc.elements.size(), 1);
    CHECK_EQ(doc.origText, str);
    CHECK(holds_alternative<generator::parse::mml::Content>(doc.elements[0]));

    auto content = std::get<generator::parse::mml::Content>(doc.elements[0]);
    CHECK(content.content == *str);
  }

  TEST_CASE("parse tag") {
    SUBCASE("Basic") {
      auto str = std::make_shared<std::string>(R"STR(~tag~)STR");
      auto res = generator::parse::mml::parse(str);
      CHECK(holds_alternative<generator::parse::mml::Document>(res));
      auto doc = std::get<generator::parse::mml::Document>(res);
      CHECK_EQ(doc.elements.size(), 1);
      CHECK_EQ(doc.origText, str);
      CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

      auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
      CHECK_EQ(tag.origText, *str);
      CHECK_EQ(tag.tagName, "tag");
      CHECK(tag.props.empty());
    }

    SUBCASE("Basic props") {
      auto str = std::make_shared<std::string>(R"STR(~tag[l=test;y=check]~)STR");
      auto res = generator::parse::mml::parse(str);
      CHECK(holds_alternative<generator::parse::mml::Document>(res));
      auto doc = std::get<generator::parse::mml::Document>(res);
      CHECK_EQ(doc.elements.size(), 1);
      CHECK_EQ(doc.origText, str);
      CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

      auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
      CHECK_EQ(tag.origText, *str);
      CHECK_EQ(tag.tagName, "tag");
      CHECK(!tag.props.empty());
      CHECK(tag.props.size() == 2);
      CHECK(tag.props["l"][0] == "test");
      CHECK(tag.props["y"][0] == "check");
    }

    SUBCASE("Repeat props") {
      auto str = std::make_shared<std::string>(R"STR(~tag[l=test;y=check;y=double;l=another;y=check]~)STR");
      auto res = generator::parse::mml::parse(str);
      CHECK(holds_alternative<generator::parse::mml::Document>(res));
      auto doc = std::get<generator::parse::mml::Document>(res);
      CHECK_EQ(doc.elements.size(), 1);
      CHECK_EQ(doc.origText, str);
      CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

      auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
      CHECK_EQ(tag.origText, *str);
      CHECK_EQ(tag.tagName, "tag");
      CHECK(!tag.props.empty());
      CHECK(tag.props.size() == 2);
      CHECK_EQ(tag.props["l"].size(), 2);
      CHECK_EQ(tag.props["y"].size(), 3);
      CHECK(tag.props["l"][0] == "test");
      CHECK(tag.props["l"][1] == "another");
      CHECK(tag.props["y"][0] == "check");
      CHECK(tag.props["y"][1] == "double");
      CHECK(tag.props["y"][2] == "check");
    }

    SUBCASE("String props - semicolon only") {
      auto str = std::make_shared<std::string>(R"STR(~tag[l=test;y="check;the;semi;colons";x=five]~)STR");
      auto res = generator::parse::mml::parse(str);
      CHECK(holds_alternative<generator::parse::mml::Document>(res));
      auto doc = std::get<generator::parse::mml::Document>(res);
      CHECK_EQ(doc.elements.size(), 1);
      CHECK_EQ(doc.origText, str);
      CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

      auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
      CHECK_EQ(tag.origText, *str);
      CHECK_EQ(tag.tagName, "tag");
      CHECK(!tag.props.empty());
      CHECK(tag.props.size() == 3);
      CHECK(tag.props["l"][0] == "test");
      CHECK(tag.props["y"][0] == "check;the;semi;colons");
      CHECK(tag.props["x"][0] == "five");
    }

    SUBCASE("String props - bracket only") {
      auto str = std::make_shared<std::string>(R"STR(~tag[l=test;y="check[and brackets]";x=five]~)STR");
      auto res = generator::parse::mml::parse(str);
      CHECK(holds_alternative<generator::parse::mml::Document>(res));
      auto doc = std::get<generator::parse::mml::Document>(res);
      CHECK_EQ(doc.elements.size(), 1);
      CHECK_EQ(doc.origText, str);
      CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

      auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
      CHECK_EQ(tag.origText, *str);
      CHECK_EQ(tag.tagName, "tag");
      CHECK(!tag.props.empty());
      CHECK(tag.props.size() == 3);
      CHECK(tag.props["l"][0] == "test");
      CHECK(tag.props["y"][0] == "check[and brackets]");
      CHECK(tag.props["x"][0] == "five");
    }

    SUBCASE("String props - semi and bracket") {
      auto str = std::make_shared<std::string>(R"STR(~tag[l=test;y="check;the;semi;colons[and brackets]";x=five]~)STR");
      auto res = generator::parse::mml::parse(str);
      CHECK(holds_alternative<generator::parse::mml::Document>(res));
      auto doc = std::get<generator::parse::mml::Document>(res);
      CHECK_EQ(doc.elements.size(), 1);
      CHECK_EQ(doc.origText, str);
      CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

      auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
      CHECK_EQ(tag.origText, *str);
      CHECK_EQ(tag.tagName, "tag");
      CHECK(!tag.props.empty());
      CHECK(tag.props.size() == 3);
      CHECK(tag.props["l"][0] == "test");
      CHECK(tag.props["y"][0] == "check;the;semi;colons[and brackets]");
      CHECK(tag.props["x"][0] == "five");
    }

    SUBCASE("Content") {
      SUBCASE("Braces") {
        auto str = std::make_shared<std::string>(R"STR(~tag{my content})STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(tag.props.empty());
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, "my content");
      }

      SUBCASE("Braces - escaped") {
        auto str = std::make_shared<std::string>(R"STR(~tag{my content\} but Escaped!})STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(tag.props.empty());
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, "my content\\} but Escaped!");
      }

      SUBCASE("Braces - nested") {
        auto str = std::make_shared<std::string>(R"STR(~tag{my content ~abc{test} but Nested!})STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(tag.props.empty());
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, "my content ");
        CHECK_EQ(std::get<generator::parse::mml::Tag>(tag.content.value()[ 1 ]).origText, "~abc{test}");
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 2 ]).content, " but Nested!");
      }

      SUBCASE("Braces - nested") {
        auto str = std::make_shared<std::string>(R"STR(~tag{my content ~abc{test} but Nested!})STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(tag.props.empty());
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, "my content ");
        CHECK_EQ(std::get<generator::parse::mml::Tag>(tag.content.value()[ 1 ]).origText, "~abc{test}");
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 2 ]).content, " but Nested!");
      }

      SUBCASE("Braces escaped nested") {
        auto str = std::make_shared<std::string>(R"STR(~tag{my content \~abc{test} but Escaped!})STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(tag.props.empty());
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, "my content \\~abc{test} but Escaped!");
      }

      SUBCASE("Default end tag") {
        auto str = std::make_shared<std::string>(R"STR(~tag
Test content
Inside
~tag~)STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(tag.props.empty());
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, R"(Test content
Inside)");
        CHECK_EQ(tag.rawContent.value(), R"(Test content
Inside
)");
      }

      SUBCASE("Default end tag with trailing text") {
        auto str = std::make_shared<std::string>(R"STR(~tag
Test content
Inside
~tag~ test
~tag~
test)STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 2);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, R"STR(~tag
Test content
Inside
~tag~ test
~tag~)STR");
        CHECK_EQ(tag.tagName, "tag");
        CHECK(tag.props.empty());
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, R"(Test content
Inside)");
        CHECK_EQ(tag.rawContent.value(), R"(Test content
Inside
~tag~ test
)");
        CHECK_EQ(std::get<generator::parse::mml::Content>(doc.elements[ 1 ]).content, "\ntest");
      }

      SUBCASE("Custom end tag") {
        auto str = std::make_shared<std::string>(R"STR(~tag[delim=CHERRY]
Test content
Inside
~CHERRY~)STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(!tag.props.empty());
        CHECK_EQ(tag.props[ "delim" ][ 0 ], "CHERRY");
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, R"(Test content
Inside)");
        CHECK_EQ(tag.rawContent.value(), R"(Test content
Inside
)");
      }

      SUBCASE("Nested block, brace") {
        auto str = std::make_shared<std::string>(R"STR(~tag[delim=CHERRY]
Test content ~eval{hello}
Inside
~CHERRY~)STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(!tag.props.empty());
        CHECK_EQ(tag.props[ "delim" ][ 0 ], "CHERRY");
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, R"(Test content )");

        auto evalTag = std::get<generator::parse::mml::Tag>(tag.content.value()[ 1 ]);
        CHECK_EQ(std::get<generator::parse::mml::Content>(evalTag.content.value()[ 0 ]).content, R"(hello)");
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 2 ]).content, R"(
Inside)");

        CHECK_EQ(evalTag.tagName, R"(eval)");
        CHECK_EQ(tag.rawContent.value(), R"(Test content ~eval{hello}
Inside
)");
      }

      SUBCASE("Nested block - block") {
        auto str = std::make_shared<std::string>(R"STR(~tag[delim=CHERRY]
Test content
  ~eval
    hello
  ~eval~
Inside
~CHERRY~)STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(!tag.props.empty());
        CHECK_EQ(tag.props[ "delim" ][ 0 ], "CHERRY");
        auto c = std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content;
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, R"(Test content
 )");

        auto evalTag = std::get<generator::parse::mml::Tag>(tag.content.value()[ 1 ]);
        auto h       = std::get<generator::parse::mml::Content>(evalTag.content.value()[ 0 ]).content;
        CHECK_EQ(std::get<generator::parse::mml::Content>(evalTag.content.value()[ 0 ]).content, R"(    hello
 )");
        auto i = std::get<generator::parse::mml::Content>(tag.content.value()[ 2 ]).content;
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 2 ]).content, R"(
Inside)");

        CHECK_EQ(evalTag.tagName, R"(eval)");
        CHECK_EQ(tag.rawContent.value(), R"(Test content
  ~eval
    hello
  ~eval~
Inside
)");
      }

      SUBCASE("Nested block - same name block") {
        auto str = std::make_shared<std::string>(R"STR(~tag
Test content
~tag
hello
~tag~
Inside
~tag~)STR");
        auto res = generator::parse::mml::parse(str);
        CHECK(holds_alternative<generator::parse::mml::Document>(res));
        auto doc = std::get<generator::parse::mml::Document>(res);
        CHECK_EQ(doc.elements.size(), 1);
        CHECK_EQ(doc.origText, str);
        CHECK(holds_alternative<generator::parse::mml::Tag>(doc.elements[ 0 ]));

        auto tag = std::get<generator::parse::mml::Tag>(doc.elements[ 0 ]);
        CHECK_EQ(tag.origText, *str);
        CHECK_EQ(tag.tagName, "tag");
        CHECK(tag.props.empty());
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 0 ]).content, R"(Test content)");

        auto evalTag = std::get<generator::parse::mml::Tag>(tag.content.value()[ 1 ]);
        CHECK_EQ(std::get<generator::parse::mml::Content>(evalTag.content.value()[ 0 ]).content, R"(hello)");
        CHECK_EQ(std::get<generator::parse::mml::Content>(tag.content.value()[ 2 ]).content, R"(
Inside)");

        CHECK_EQ(evalTag.tagName, R"(tag)");
        CHECK_EQ(tag.rawContent.value(), R"STR(Test content
~tag
hello
~tag~
Inside
)STR");
      }

      SUBCASE("INVALID PARSES") {

        SUBCASE("Nested block - line") {
          auto str = std::make_shared<std::string>(R"STR(~tag[delim=CHERRY]
Test content ~eval hello
Inside
          ~CHERRY~)STR");
          auto res = generator::parse::mml::parse(str);
          CHECK(holds_alternative<generator::parse::mml::ParseError>(res));
        }

        SUBCASE("End Of File") {
          const auto* str = R"STR(~tag my content})STR";
          auto        res = generator::parse::mml::parse(str);
          CHECK(holds_alternative<generator::parse::mml::ParseError>(res));
        }

        SUBCASE("End Of File No Nested") {
          const auto* str = R"STR(~tag my content ~tag~})STR";
          auto        res = generator::parse::mml::parse(str);
          CHECK(holds_alternative<generator::parse::mml::ParseError>(res));
        }

        SUBCASE("End Of Line") {
          auto str = std::string(R"STR(~tag my content}
        ))STR");
          auto res = generator::parse::mml::parse(str);
          CHECK(holds_alternative<generator::parse::mml::ParseError>(res));
        }

        SUBCASE("No newlines") {
          auto str = std::string(R"STR(~tag my content ~tag~}
        ))STR");
          auto res = generator::parse::mml::parse(str);
          CHECK(holds_alternative<generator::parse::mml::ParseError>(res));
        }
      }
    }
  }

//  TEST_CASE("parse doc") {
//    auto res = generator::parse::mml::parse(R"STR(~sec[l=2] Sub Heading ~eval{{(-> (info) (get :cur_header) (get 3000))}
//
//This is a sub heading with a dynamic title which is the heading number. It is using the 3000th part,
//which doesn't exist. However, if it did exist, then it would be "0" so we're returning 0.
//
//~code[lang=js]
//alert('Custom JavaScript embedded in the code!)'
//~code~
//    )STR");
//    CHECK(holds_alternative<generator::parse::mml::Document>(res));
//    auto doc = std::get<generator::parse::mml::Document>(res);
//    CHECK_EQ(doc.elements.size(), 4);
//    CHECK(holds_alternative<generator::parse::mml::Content>(doc.elements[0]));
//  }
}