#include <flt/string/longest_common_subsequence.hpp>
#include <flt/string/word_iterator.hpp>

#include <array>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace flt::string::algorithms;
  using namespace flt::string::iterator;
  using namespace std::literals::string_literals;

  auto myA = "scclawn"s;
  auto myB = "lawn"s;
  auto myC = "wanwmn"s;
  auto myD = "lwnmower"s;
  using paired_string_it =
      std::pair<std::string::iterator, std::string::iterator>;
  auto it_vec = std::vector<paired_string_it>{};
  it_vec.emplace_back(std::begin(myA), std::end(myA));
  it_vec.emplace_back(std::begin(myB), std::end(myB));
  it_vec.emplace_back(std::begin(myC), std::end(myC));
  it_vec.emplace_back(std::begin(myD), std::end(myD));

  const auto LCS = longest_common_subsequence(
      std::begin(it_vec), std::end(it_vec), std::equal_to<>());

  std::cout << "LCS of '" << myA << "', '" << myB << "', '" << myC << " and '"
            << myD << "' is: ";
  for (auto&& word : LCS)
    std::cout << word;

  std::cout << std::endl;

  auto myAs = "clawn is my lawn so far"s;
  auto myBs = "clawn lawn thus far"s;
  auto itAb = cbegin_wordwise(myAs);
  auto itAe = cend_wordwise(myAs);
  auto itBb = cbegin_wordwise(myBs);
  auto itBe = cend_wordwise(myBs);

  using paired_word_it = std::array<decltype(itAb), 2>;
  auto it_word_vec = std::vector<paired_word_it>{};
  it_word_vec.emplace_back(paired_word_it{itAb, itAe});
  it_word_vec.emplace_back(paired_word_it{itBb, itBe});

  const auto LCSword = longest_common_subsequence(
      std::begin(it_word_vec), std::end(it_word_vec), std::equal_to<>());

  std::cout << "LCS of '" << myAs << "' and '" << myBs << "' is: ";
  for (auto&& word : LCSword)
    std::cout << word << ' ';
  std::cout << std::endl;
}
