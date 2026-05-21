#include <string>

#include "ndsort/ndsort.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Name is ndsort", "[library]")
{
  auto const exported = exported_class {};
  REQUIRE(std::string("ndsort") == exported.name());
}
