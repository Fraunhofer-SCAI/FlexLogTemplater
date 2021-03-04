#include <flt/string/levenshtein_distance.hpp>
#include <flt/string/weighted_edit_distance.hpp>
#include <flt/string/word_iterator.hpp>
#include <flt/util/cache_adaptors.hpp>
#include <flt/util/sigmoids.hpp>

#include <iostream>
#include <string>
#include <string_view>

int main() {
  using flt::string::levenshtein_distance;
  using flt::string::weighted_edit_distance;
  using flt::util::logistic_decrease;
  using namespace flt::string::iterator;

  using namespace std::string_literals;
  using namespace std::string_view_literals;

  const auto test_string1 = "this is a test string"s;
  const auto test_string2 = "this is a fest string"sv;

  auto weight_f = [](const auto k) { return logistic_decrease(k); };
  auto dist_f = [](const auto& a, const auto& b) {
    return levenshtein_distance(a, b);
  };
  auto wed_f = [&weight_f, &dist_f](const auto& a, const auto& b) {
    return weighted_edit_distance(cbegin_wordwise(a), cend_wordwise(a),
                                  cbegin_wordwise(b), cend_wordwise(b), dist_f,
                                  weight_f);
  };

  std::cout << "WED is " << wed_f(test_string1, test_string2) << std::endl;

  {
    using flt::util::ordered_cache_adaptor;
    auto k = ordered_cache_adaptor<decltype(wed_f), std::string_view,
                                   std::string_view>{wed_f};
    std::cout << "1+2 = " << k("help"sv, "halp"sv) << std::endl;
    std::cout << "3+4 = " << k("help"sv, "help"sv) << std::endl;
    std::cout << "1+2 = " << k("halp"sv, "help"sv) << std::endl;
  }

  {
    using flt::util::symmetric_cache_adaptor;
    auto k = symmetric_cache_adaptor<decltype(wed_f), std::string_view,
                                     std::string_view>{wed_f};
    std::cout << "1+2 = " << k("help"sv, "halp"sv) << std::endl;
    std::cout << "3+4 = " << k("help"sv, "help"sv) << std::endl;
    std::cout << "1+2 = " << k("halp"sv, "help"sv) << std::endl;
  }
}
