#ifndef FLT_CLUSTERING_CLUSTER_UTILS_HPP
#define FLT_CLUSTERING_CLUSTER_UTILS_HPP

#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace flt::clustering {
template <typename T, typename SetT, typename F>
auto find_minimum_distance(const T& x, const SetT& nodes, F&& dist_f) {
  using node_t = std::decay_t<T>;
  using nodes_elem_t = typename SetT::value_type;
  static_assert(std::is_invocable_v<F, node_t, nodes_elem_t>,
                "Cannot invoke edge function with an argument of two nodes");
  using edge_result_t = std::invoke_result_t<F, node_t, nodes_elem_t>;
  static_assert(std::is_arithmetic_v<edge_result_t>,
                "Edge function does not return an arithmetic type");

  auto min_distance = edge_result_t{};
  if constexpr (std::numeric_limits<edge_result_t>::has_infinity) {
    min_distance = std::numeric_limits<edge_result_t>::infinity();
  } else {
    min_distance = std::numeric_limits<edge_result_t>::max();
  }
  auto min_cl = std::cbegin(nodes);
  for (auto it = std::cbegin(nodes), end_it = std::cend(nodes); it != end_it;
       ++it) {
    if (const auto dist_xy = dist_f(x, *it); dist_xy < min_distance) {
      min_distance = dist_xy;
      min_cl = it;
    }
  }
  return std::tuple{std::move(min_distance), std::move(min_cl)};
}

template <typename T, typename SetSetT, typename F>
auto find_minimum_distance_cluster(const T& x, const SetSetT& clusters,
                                   F&& dist_f) {
  return find_minimum_distance(x, clusters, [&](auto&& a, auto&& b) {
    return std::get<0>(find_minimum_distance(a, b, dist_f));
  });
}
} // namespace flt::clustering

#endif
