#include <flt/string/longest_common_subsequence.hpp>
#include <flt/string/word_iterator.hpp>

#include <array>
#include <fstream>
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

  auto vec_lines = std::vector<std::string>{};

  for (auto lines = 0; lines < 100; ++lines) {
    auto str = std::string{};
    for (auto priv = 0; priv < 2; ++priv) {
      str += " com " + std::to_string(lines);
    }
    vec_lines.emplace_back(str);
  }

  {
    auto fout = std::ofstream{"test_lcs_complex_lines"};
    for (auto&& line : vec_lines) {
      fout << line << '\n';
    }
    fout.flush();
  }

  using paired_word_it = std::array<word_iterator<std::string>, 2>;
  auto it_word_vec = std::vector<paired_word_it>{};

  for (auto&& line : vec_lines) {
    auto it_b = cbegin_wordwise(line);
    auto it_e = cend_wordwise(line);
    it_word_vec.emplace_back(paired_word_it{it_b, it_e});
  }

  const auto lcs_word = longest_common_subsequence(
      std::begin(it_word_vec), std::end(it_word_vec), std::equal_to<>());

  std::cout << "LCS is: ";
  for (auto&& word : lcs_word)
    std::cout << word << '|';
  std::cout << std::endl;
}
