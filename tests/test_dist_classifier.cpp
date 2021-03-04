#include <flt/clustering/distance_classifier.hpp>

#include <iostream>
#include <set>

int main() {
  using flt::clustering::distance_classification_threshold_sorted;
  auto set_1 = std::set{1., 2.};
  const auto cutoff_1 = distance_classification_threshold_sorted(
      std::begin(set_1), std::end(set_1));
  std::cout << "Cutoff for set_1: " << cutoff_1 << std::endl;

  auto set_2 = std::set{1, 2};
  const auto cutoff_2 = distance_classification_threshold_sorted(
      std::begin(set_2), std::end(set_2));
  std::cout << "Cutoff for set_2: " << cutoff_2 << std::endl;

  auto set_3 = std::set{1, 2, 2};
  const auto cutoff_3 = distance_classification_threshold_sorted(
      std::begin(set_3), std::end(set_3));
  std::cout << "Cutoff for set_3: " << cutoff_3 << std::endl;

  auto set_4 = std::set{1, 1, 2};
  const auto cutoff_4 = distance_classification_threshold_sorted(
      std::begin(set_4), std::end(set_4));
  std::cout << "Cutoff for set_4: " << cutoff_4 << std::endl;

  auto set_5 = std::set{3, 5, 9}; // Initial cutoff is correct
  const auto cutoff_5 = distance_classification_threshold_sorted(
      std::begin(set_5), std::end(set_5));
  std::cout << "Cutoff for set_5: " << cutoff_5 << std::endl;
}
