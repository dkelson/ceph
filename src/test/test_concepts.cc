#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include "include/concepts.h"

#include <array>
#include <deque>
#include <forward_list>
#include <initializer_list>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using ceph::concepts::associative_container;
using ceph::concepts::can_append;
using ceph::concepts::can_append_range;
using ceph::concepts::can_erase_if;
using ceph::concepts::can_prepend;
using ceph::concepts::can_remove_front;
using ceph::concepts::container_compatible_range;
using ceph::concepts::contiguous_sequence;
using ceph::concepts::sequence;
using ceph::concepts::unordered_container;

// Helper gadgets for the unit tests:
namespace {

struct vector_backed_range {
  using value_type = int;
  using iterator = std::vector<int>::iterator;
  using const_iterator = std::vector<int>::const_iterator;

  std::vector<int> values;

  vector_backed_range() = default;
  vector_backed_range(std::initializer_list<int> init) : values(init) {}

  iterator begin() { return values.begin(); }
  iterator end() { return values.end(); }
  const_iterator begin() const { return values.begin(); }
  const_iterator end() const { return values.end(); }
};

// Expose iterator interface to prove can_append() and push_back() will
// fall back to insert() when push_back()/emplace_back() don't exist:
struct append_only : vector_backed_range {
  using vector_backed_range::vector_backed_range;

  void insert(iterator pos, int value) {
    values.insert(pos, value);
  }
};

// Only exposes erase(I), to help verify can_remove_front() and pop_front()
// can work without pop_front() directly being available:
struct front_erasable : vector_backed_range {
  using vector_backed_range::vector_backed_range;

  iterator erase(iterator pos) {
    return values.erase(pos);
  }
};

// May be constructed from int, but does not convert TO one:
struct explicit_from_int {
  explicit explicit_from_int(int) {}
};

template <typename RangeT, typename ValueT>
struct range_compat_case {
  using range_type = RangeT;
  using value_type = ValueT;
};

using int_predicate = bool (*)(int);

bool is_even(int value)
{
  return value % 2 == 0;
}

template <typename ContainerT>
requires can_append_range<ContainerT, const std::array<int, 4>&> &&
         can_prepend<ContainerT, int> &&
         can_erase_if<ContainerT, int_predicate>
ContainerT collect_odd_values()
{
  const std::array input{1, 2, 3, 4};
  ContainerT values;

  ceph::util::maybe_reserve(values, input.size() + 1);
  ceph::util::append_range(values, input);
  ceph::util::erase_if(values, is_even);
  ceph::util::push_front(values, 0);

  return values;
}

} // namespace

/*** Tests for library general Concepts: */

TEMPLATE_PRODUCT_TEST_CASE("standard sequence containers model sequence",
                           "[concepts]",
                           (std::vector, std::list, std::forward_list),
                           (int))
{
  STATIC_REQUIRE(sequence<TestType>);
}

TEMPLATE_TEST_CASE("contiguous sequence containers model contiguous_sequence",
                   "[concepts]", std::vector<int>, (std::array<int, 3>))
{
  STATIC_REQUIRE(contiguous_sequence<TestType>);
}

TEMPLATE_TEST_CASE("compatible ranges model container_compatible_range",
                   "[concepts]",
                   (range_compat_case<std::vector<int>, int>),
                   (range_compat_case<const std::vector<int>, int>),
                   (range_compat_case<std::array<short, 3>, int>),
                   (range_compat_case<std::string, char>),
                   (range_compat_case<std::vector<int>, const int&>))
{
  STATIC_REQUIRE(container_compatible_range<typename TestType::range_type,
                                            typename TestType::value_type>);
}

TEMPLATE_TEST_CASE("ordered associative containers model associative_container",
                   "[concepts]", std::set<int>, std::multiset<int>,
                   (std::map<int, std::string>),
                   (std::multimap<int, std::string>))
{
  STATIC_REQUIRE(associative_container<TestType>);
}

TEMPLATE_TEST_CASE("unordered associative containers model unordered_container",
                   "[concepts]", std::unordered_set<int>,
                   std::unordered_multiset<int>,
                   (std::unordered_map<int, std::string>),
                   (std::unordered_multimap<int, std::string>))
{
  STATIC_REQUIRE(unordered_container<TestType>);
}

/*** Tests for aggregate capability checks: */

TEMPLATE_TEST_CASE("appendable containers model can_append", "[concepts]",
                   std::vector<int>, std::deque<int>, std::list<int>,
                   std::set<int>, append_only)
{
  STATIC_REQUIRE(can_append<TestType, int>);
}

TEMPLATE_PRODUCT_TEST_CASE("prependable containers model can_prepend",
                           "[concepts]",
                           (std::deque, std::list, std::forward_list,
                            std::vector),
                           (int))
{
  STATIC_REQUIRE(can_prepend<TestType, int>);
}

TEMPLATE_PRODUCT_TEST_CASE("standard range appendable containers model "
                           "can_append_range",
                           "[concepts]", (std::vector, std::set), (int))
{
  STATIC_REQUIRE(can_append_range<TestType, std::array<int, 3>>);
}

TEMPLATE_PRODUCT_TEST_CASE("predicate erasable containers model can_erase_if",
                           "[concepts]",
                           (std::vector, std::list, std::forward_list),
                           (int))
{
  STATIC_REQUIRE(can_erase_if<TestType, int_predicate>);
}

TEMPLATE_PRODUCT_TEST_CASE("standard front removable containers model "
                           "can_remove_front",
                           "[concepts]",
                           (std::deque, std::list, std::forward_list,
                            std::vector),
                           (int))
{
  STATIC_REQUIRE(can_remove_front<TestType>);
}

TEST_CASE("concept edge cases",
          "[concepts]")
{
  STATIC_REQUIRE_FALSE(contiguous_sequence<std::deque<int>>);
  STATIC_REQUIRE_FALSE(container_compatible_range<std::vector<int>,
                                                  std::string>);
  STATIC_REQUIRE_FALSE(container_compatible_range<std::vector<int>,
                                                  explicit_from_int>);
  STATIC_REQUIRE_FALSE(container_compatible_range<int, int>);
  STATIC_REQUIRE_FALSE(associative_container<std::vector<int>>);
  STATIC_REQUIRE_FALSE(unordered_container<std::set<int>>);
  STATIC_REQUIRE_FALSE(can_append<std::forward_list<int>, int>);
  STATIC_REQUIRE(can_append_range<append_only, std::array<int, 3>>);
  STATIC_REQUIRE_FALSE(can_append_range<std::forward_list<int>,
                                        std::array<int, 3>>);
  STATIC_REQUIRE(can_remove_front<front_erasable>);
}

/*** Tests for library helpers: */

TEMPLATE_PRODUCT_TEST_CASE("helpers compose into container-generic algorithms",
                           "[concepts][util]",
                           (std::deque, std::list, std::vector),
                           (int))
{
  const TestType expected{0, 1, 3};

  CHECK(collect_odd_values<TestType>() == expected);
}

TEST_CASE("push_back appends through the best available container operation",
          "[concepts][util]")
{
  SECTION("vector uses back insertion") {
    std::vector<int> values;

    ceph::util::push_back(values, 1);
    ceph::util::push_back(values, 2);

    CHECK(values == std::vector{1, 2});
  }

  SECTION("set falls back to hinted insertion at end") {
    std::set<int> values;

    ceph::util::push_back(values, 2);
    ceph::util::push_back(values, 1);

    CHECK(values == std::set{1, 2});
  }

  SECTION("custom append-only type uses insert(end, value)") {
    append_only values;

    ceph::util::push_back(values, 1);
    ceph::util::push_back(values, 2);

    CHECK(values.values == std::vector{1, 2});
  }
}

TEST_CASE("append_range appends ranges with container-specific fallbacks",
          "[concepts][util]")
{
  SECTION("vector appends iterator ranges") {
    std::vector<int> values{1};
    const std::array more{2, 3};

    ceph::util::append_range(values, more);

    CHECK(values == std::vector{1, 2, 3});
  }

  SECTION("set inserts an input range") {
    std::set<int> values{1};
    const std::array more{3, 2};

    ceph::util::append_range(values, more);

    CHECK(values == std::set{1, 2, 3});
  }

  SECTION("custom append-only type falls back to element appends") {
    append_only values;
    const std::array more{1, 2, 3};

    ceph::util::append_range(values, more);

    CHECK(values.values == std::vector{1, 2, 3});
  }
}

TEST_CASE("insert_range inserts at the requested position", "[concepts][util]")
{
  std::vector<int> values{1, 4};
  const std::array more{2, 3};

  ceph::util::insert_range(values, values.begin() + 1, more);

  CHECK(values == std::vector{1, 2, 3, 4});
}

TEST_CASE("erase_if removes matching values through available operations",
          "[concepts][util]")
{
  SECTION("vector uses erase/remove_if") {
    std::vector<int> values{1, 2, 3, 4};

    ceph::util::erase_if(values, is_even);

    CHECK(values == std::vector{1, 3});
  }

  SECTION("list uses remove_if") {
    std::list<int> values{1, 2, 3, 4};

    ceph::util::erase_if(values, is_even);

    CHECK(values == std::list{1, 3});
  }

  SECTION("forward_list uses remove_if") {
    std::forward_list<int> values{1, 2, 3, 4};

    ceph::util::erase_if(values, is_even);

    CHECK(values == std::forward_list{1, 3});
  }
}

TEST_CASE("front helpers use front-specific operations or begin erasure",
          "[concepts][util]")
{
  SECTION("push_front prepends values") {
    std::vector<int> values{2, 3};

    ceph::util::push_front(values, 1);

    CHECK(values == std::vector{1, 2, 3});
  }

  SECTION("pop_front removes the first value") {
    front_erasable values{{1, 2, 3}};

    ceph::util::pop_front(values);

    CHECK(values.values == std::vector{2, 3});
  }
}

TEST_CASE("optional sizing helpers call supported operations only",
          "[concepts][util]")
{
  std::vector<int> values;

  ceph::util::maybe_reserve(values, 4);
  ceph::util::maybe_resize(values, 3);

  CHECK(ceph::util::capacity(values) >= 4);
  CHECK(ceph::util::size(values) == 3);
  CHECK(!ceph::util::empty(values));

  ceph::util::clear(values);

  CHECK(ceph::util::empty(values));
}
