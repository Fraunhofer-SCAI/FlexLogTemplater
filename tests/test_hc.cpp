#include <flt/util/hash_combine.hpp>

#include <iostream>
#include <string_view>
#include <tuple>

int main() {
  using flt::util::multi_hash;
  using flt::util::ordered_tuple_hash;
  using flt::util::symmetric_multi_hash;

  using namespace std::literals;

  const auto test_string1 = "this is a test string "sv;
  const auto test_string2 = "what a long day"sv;
  const auto test_string3 = "what a long say"sv;

  std::cout << multi_hash(test_string1, test_string2) << std::endl;
  std::cout << multi_hash(test_string1, test_string3) << std::endl;
  std::cout << symmetric_multi_hash(test_string2, test_string3) << std::endl;
  std::cout << symmetric_multi_hash(test_string3, test_string2) << std::endl;

  const auto test_tuple =
      std::make_tuple(test_string1, test_string2, test_string3);
  std::cout << ordered_tuple_hash{}(test_tuple) << std::endl;
}
