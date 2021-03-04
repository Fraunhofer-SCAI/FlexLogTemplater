#include <flt/clustering/agglomerative.hpp>
#include <flt/clustering/cluster_utils.hpp>
#include <flt/string/levenshtein_distance.hpp>
#include <flt/string/weighted_edit_distance.hpp>
#include <flt/string/word_iterator.hpp>
#include <flt/util/cache_adaptors.hpp>
#include <flt/util/sigmoids.hpp>

#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_set>

int main() {
  using flt::string::levenshtein_distance;
  using flt::string::weighted_edit_distance;
  using flt::util::logistic_decrease;
  using flt::util::symmetric_cache_adaptor;
  using flt::util::symmetric_sync_cache_adaptor;
  using namespace flt::string::iterator;

  using namespace std::literals;

  std::ios_base::sync_with_stdio(false);
  auto v = std::unordered_set<std::string_view>{
      {"Hallo Peter"sv, "Hallo Heter"sv, "Hallo Karl"sv, "Hallp Peter"sv}};

  auto weight_f = [](const auto k) { return logistic_decrease(k); };
  auto dist_f = [](const auto& a, const auto& b) {
    return levenshtein_distance(a, b);
  };
  auto&& cached_dist_f =
      symmetric_sync_cache_adaptor<decltype(dist_f), std::string_view,
                                   std::string_view>{dist_f};
  auto wed_f = [&](const auto& a, const auto& b) {
    return weighted_edit_distance(
        cbegin_wordwise(a), cend_wordwise(a), cbegin_wordwise_quoted(b),
        cend_wordwise_quoted(b), cached_dist_f, weight_f);
  };

  auto edge_f = [&](auto a, auto b) { return (wed_f(a, b) < 0.25); };
  auto r = std::vector<decltype(v)>{};
  flt::clustering::agglomerative_clustering(std::back_inserter(r), v, edge_f);
  auto counter = 0;
  for (auto&& clust : r) {
    for (auto&& line : clust) {
      std::cout << line << " => " << counter << std::endl;
      std::cout << "mini is "
                << std::get<0>(flt::clustering::find_minimum_distance_cluster(
                       line, r, wed_f))
                << std::endl;
    }
    ++counter;
  }
  auto x = "Hallo Pter"sv;
  auto [d, p] = flt::clustering::find_minimum_distance_cluster(x, r, wed_f);
  std::cout << "Minimal distance to \"" << x << "\" has d = " << d << std::endl;
}
