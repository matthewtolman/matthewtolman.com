#include "doctest.h"
#include "str_utils.h"

TEST_SUITE("str_utils") {
  TEST_CASE("find_not_escaped") {
    std::string string = R"STR(Marry \t had a little lamb,\n little lamb. Marry had a \n little \" lamb who\'s fleece was white as snow!)STR";
    SUBCASE("No occurrence") {
      CHECK_EQ(string.end(), str_utils::find_not_escaped(string.begin(), string.end(), '$'));
    }

    SUBCASE("No unescaped occurence") {
      CHECK_EQ(string.end(), str_utils::find_not_escaped(string.begin(), string.end(), '\''));
    }

    SUBCASE("Escaped after unescaped") {
      auto occurrence = str_utils::find_not_escaped(string.begin(), string.end(), 't');
      CHECK_NE(string.end(), occurrence);
      CHECK_EQ(occurrence - string.begin(), 17);
    }

    SUBCASE("No previous escape") {
      auto occurrence = str_utils::find_not_escaped(string.begin(), string.end(), 'a');
      CHECK_NE(string.end(), occurrence);
      CHECK_EQ(occurrence - string.begin(), 1);
    }

    SUBCASE("Custom Escape") {
      auto occurrence = str_utils::find_not_escaped(string.begin(), string.end(), 'r', 'a');
      CHECK_NE(string.end(), occurrence);
      CHECK_EQ(occurrence - string.begin(), 3);
    }
  }

  TEST_CASE("find_not_quoted") {
    std::string string = R"STR(Jack was "nimble" "jack was \" quick$" jack jumped over "the candlestick"$)STR";
    SUBCASE("No occurrence") {
      CHECK_EQ(string.end(), str_utils::find_not_quoted(string.begin(), string.end(), 'z'));
    }

    SUBCASE("No unquoted occurrence") {
      CHECK_EQ(string.end(), str_utils::find_not_quoted(string.begin(), string.end(), 'q'));
    }

    SUBCASE("Unquoted after quoted") {
      CHECK_EQ(46, str_utils::find_not_quoted(string.begin(), string.end(), 'm') - string.begin());
    }

    SUBCASE("No previously quoted") {
      CHECK_EQ(3, str_utils::find_not_quoted(string.begin(), string.end(), 'k') - string.begin());
    }

    SUBCASE("Unquoted after escaped quoted") {
      CHECK_EQ(73, str_utils::find_not_quoted(string.begin(), string.end(), '$') - string.begin());
    }
  }

  TEST_CASE("range_to_view") {
    std::string orig = "Hello world!";
    CHECK_EQ(str_utils::range_to_view(orig.begin(), orig.begin() + 5), "Hello");
    CHECK_EQ(str_utils::range_to_view(orig.begin() + 6, orig.end() - 5), "w");

    CHECK_EQ(str_utils::range_to_view(std::make_tuple(orig.begin(), orig.begin() + 5)), "Hello");
    CHECK_EQ(str_utils::range_to_view(std::make_tuple(orig.begin() + 6, orig.end() - 5)), "w");
  }

  TEST_CASE("find_substr") {
    std::string string = "Hello world, how are you?";

    SUBCASE("Not present") {
      auto [begin, end] = str_utils::find_substr(string.begin(), string.end(), "world how");
      CHECK_EQ(begin, string.end());
      CHECK_EQ(end, string.end());
      CHECK_EQ(begin, str_utils::find_substr_start(string.begin(), string.end(), "world how"));
    }

    SUBCASE("At start") {
      auto [begin, end] = str_utils::find_substr(string.begin(), string.end(), "Hello");
      CHECK_EQ(begin, string.begin());
      CHECK_EQ(end, string.begin() + 5);
      CHECK_EQ(begin, str_utils::find_substr_start(string.begin(), string.end(), "Hello"));
    }

    SUBCASE("At end") {
      auto [begin, end] = str_utils::find_substr(string.begin(), string.end(), "you?");
      CHECK_EQ(begin, string.end() - 4);
      CHECK_EQ(end, string.end());
      CHECK_EQ(begin, str_utils::find_substr_start(string.begin(), string.end(), "you?"));
    }

    SUBCASE("in the middle") {
      auto [begin, end] = str_utils::find_substr(string.begin(), string.end(), ", ");
      CHECK_EQ(begin, string.begin() + 11);
      CHECK_EQ(end, string.begin() + 13);
      CHECK_EQ(begin, str_utils::find_substr_start(string.begin(), string.end(), ", "));
    }

    SUBCASE("repeated, grabs first") {
      auto [begin, end] = str_utils::find_substr(string.begin(), string.end(), " ");
      CHECK_EQ(begin, string.begin() + 5);
      CHECK_EQ(end, string.begin() + 6);
      CHECK_EQ(begin, str_utils::find_substr_start(string.begin(), string.end(), " "));
    }

    SUBCASE("Case sensitive") {
      auto [begin, end] = str_utils::find_substr(string.begin(), string.end(), "hello");
      CHECK_EQ(begin, string.end());
      CHECK_EQ(end, string.end());
      CHECK_EQ(begin, str_utils::find_substr_start(string.begin(), string.end(), "hello"));
    }
  }

  TEST_CASE("find_substr_followed_by_eof_or_newline") {
    std::string string = R"STR(Hello world
how are you?
doing today)STR";

    SUBCASE("Not Present") {
      auto [begin, end] = str_utils::find_substr_followed_by_eof_or_newline(string.begin(), string.end(), "target");
      CHECK_EQ(begin, string.end());
      CHECK_EQ(end, string.end());
    }

    SUBCASE("No trailing newline") {
      auto [begin, end] = str_utils::find_substr_followed_by_eof_or_newline(string.begin(), string.end(), "Hello");
      CHECK_EQ(begin, string.end());
      CHECK_EQ(end, string.end());
    }

    SUBCASE("trailing newline") {
      auto [begin, end] = str_utils::find_substr_followed_by_eof_or_newline(string.begin(), string.end(), "you?");
      CHECK_EQ(begin - string.begin(), 20);
      CHECK_EQ(end - string.begin(), 24);
    }

    SUBCASE("end of file") {
      auto [begin, end] = str_utils::find_substr_followed_by_eof_or_newline(string.begin(), string.end(), "day");
      CHECK_EQ(begin - string.begin(), 33);
      CHECK_EQ(end, string.end());
    }
  }

  TEST_CASE("find_not_escaped_stack") {
    SUBCASE("Plain") {
      std::string string = "}{{}{{{}}{}}{}}{}{{}}}}";
      auto        ch     = str_utils::find_not_escaped_stack(string.begin(), string.end(), '}', '{');
      CHECK_EQ(ch - string.begin(), 0);
    }

    SUBCASE("Pairs") {
      std::string string = "{{}{{{}}{}}{}}{}{{}}}}";
      auto        ch     = str_utils::find_not_escaped_stack(string.begin(), string.end(), '}', '{');
      CHECK_EQ(ch - string.begin(), 13);
    }

    SUBCASE("Escaped") {
      std::string string = "\\}{{}{{{}}{}}{}}{}{{}}}}";
      auto        ch     = str_utils::find_not_escaped_stack(string.begin(), string.end(), '}', '{');
      CHECK_EQ(ch - string.begin(), 15);
    }

    SUBCASE("Escaped Pairs") {
      std::string string = "{\\{}{{{}}{}}{}}{}{{}}}}";
      auto        ch     = str_utils::find_not_escaped_stack(string.begin(), string.end(), '}', '{');
      CHECK_EQ(ch - string.begin(), 3);
    }

    SUBCASE("Escaped With Escaped Pairs") {
      std::string string = "{\\{\\}{{{}}{}}{}}{}{{}}}}";
      auto        ch     = str_utils::find_not_escaped_stack(string.begin(), string.end(), '}', '{');
      CHECK_EQ(ch - string.begin(), 15);
    }
  }
}
