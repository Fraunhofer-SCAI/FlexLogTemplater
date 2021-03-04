#include <flt/string/word_iterator.hpp>

#include <iostream>
#include <string_view>

int main() {
  using flt::string::cbegin_wordwise;
  using flt::string::cend_wordwise;

  using namespace std::string_view_literals;

  const auto schlurp1 = "this is a test string "sv;

  auto c1 = cbegin_wordwise(schlurp1);
  auto c2 = cend_wordwise(schlurp1);

  for (; c1 != c2; ++c1)
    std::cout << *c1 << std::endl;
}
