#include <flt/string/levenshtein_distance.hpp>
#include <flt/string/weighted_edit_distance.hpp>
#include <flt/string/word_iterator.hpp>
#include <flt/util/sigmoids.hpp>

#include <iostream>
#include <string_view>

int main() {
  using flt::string::levenshtein_distance;
  using flt::string::weighted_edit_distance;
  using flt::util::logistic_decrease;
  using namespace flt::string::iterator;

  using namespace std::string_view_literals;

  const auto test_string1 = "this is a test string"sv;
  const auto test_string2 = "this is a fest string"sv;

  auto weight_f = [](const auto k) { return logistic_decrease(k); };
  auto dist_f = [](const auto& a, const auto& b) {
    return levenshtein_distance(a, b);
  };

  std::cout << "WED is "
            << weighted_edit_distance(
                   cbegin_wordwise(test_string1), cend_wordwise(test_string1),
                   cbegin_wordwise(test_string2), cend_wordwise(test_string2),
                   dist_f, weight_f)
            << std::endl;
}
