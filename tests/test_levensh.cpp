#include <flt/string/levenshtein_path.hpp>
#include <flt/string/word_iterator.hpp>

#include <functional>
#include <iostream>
#include <string>

int main() {
  using namespace flt::string::distances;
  using namespace flt::string::iterator;
  using namespace std::literals::string_literals;

  auto myA = "claw"s;
  auto myB = "lawn"s;
  const auto [d, path] = levenshtein_path(myA, myB);

  std::cout << "Levenshtein from " << myA << " to " << myB << " is " << d
            << std::endl
            << "Path is : ";

  for (auto&& pe : path) {
    switch (pe) {
    case levenshtein_edit_op::retention:
      std::cout << "R";
      break;
    case levenshtein_edit_op::substitution:
      std::cout << "S";
      break;
    case levenshtein_edit_op::insertion:
      std::cout << "I";
      break;
    case levenshtein_edit_op::deletion:
      std::cout << "D";
      break;
    }
  }
  auto myAs = "clawn is my lawn so far"s;
  auto myBs = "clawn lawn thus far"s;
  auto itAb = cbegin_wordwise(myAs);
  auto itAe = cend_wordwise(myAs);
  auto itBb = cbegin_wordwise(myBs);
  auto itBe = cend_wordwise(myBs);
  const auto [d1, path1] =
      levenshtein_path(itAb, itAe, itBb, itBe, std::equal_to<>());

  std::cout << std::endl
            << "Levenshtein from " << myAs << " to " << myBs << " is " << d1
            << std::endl;
  for (auto&& pe : path1) {
    switch (pe) {
    case levenshtein_edit_op::retention:
      std::cout << "R";
      break;
    case levenshtein_edit_op::substitution:
      std::cout << "S";
      break;
    case levenshtein_edit_op::insertion:
      std::cout << "I";
      break;
    case levenshtein_edit_op::deletion:
      std::cout << "D";
      break;
    }
  }
  std::cout << std::endl;
}
