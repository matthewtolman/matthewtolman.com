#include "doctest.h"
#include "files.h"
#include <filesystem>

TEST_SUITE("generator::files") {
  TEST_CASE("get loadable fiels") {
    using path = std::filesystem::path;
    auto curDir   = path(__FILE__).parent_path();
    auto testDir = curDir / "sample_blog_files";
    auto expected = std::vector<std::filesystem::path>{testDir / "blog.mml"};
    auto actual = generator::files::get_loadable_files(testDir);
    CHECK_EQ(expected.size(), actual.size());
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());

    for(auto eIter = expected.begin(), aIter = actual.begin(); eIter != expected.end() && aIter != actual.end(); ++eIter, ++aIter) {
      CHECK_EQ(*eIter, *aIter);
    }
  }
}
