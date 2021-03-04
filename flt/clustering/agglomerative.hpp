#ifndef FLT_CLUSTERING_AGGLOMERATIVE_HPP
#define FLT_CLUSTERING_AGGLOMERATIVE_HPP

#include <flt/util/type_utils.hpp>

#include <algorithm>
#include <cassert>
#include <future>
#include <iterator>
#include <mutex>
#include <queue>
#include <type_traits>

namespace flt::clustering {
inline namespace agglomerative {
template <typename T, typename SetT, typename F>
auto bfs(T first_node, const SetT& nodes, F&& edge_f) {
  using node_t = std::decay_t<T>;
  static_assert(std::is_convertible_v<node_t, typename SetT::value_type>,
                "T is not compatible to the node set");
  static_assert(std::is_invocable_v<F, node_t, node_t>,
                "Cannot invoke edge function with an argument of two nodes");
  using edge_result_t = std::invoke_result_t<F, node_t, node_t>;
  static_assert(std::is_arithmetic_v<edge_result_t>,
                "Edge function does not return an arithmetic type");

  auto connected_nodes = SetT{first_node};
  auto query_nodes = std::queue<node_t>{};
  query_nodes.push(std::move(first_node));

  while (!std::empty(query_nodes)) {
    auto front_node = query_nodes.front();
    query_nodes.pop();

    for (auto&& node : nodes) {
      if (connected_nodes.find(node) == std::end(connected_nodes) &&
          edge_f(front_node, node)) {
        connected_nodes.insert(node);
        query_nodes.push(node);
      }
    }
  }
  return std::move(connected_nodes);
}

template <typename OutputIt, typename SetT, typename F,
          typename T = typename SetT::value_type>
auto agglomerative_clustering(OutputIt outit, SetT nodes, F&& edge_f) {
  static_assert(flt::util::is_output_iterator<OutputIt>(),
                "OutputIt needs to be an OutputIterator");
  const auto max_threads = std::max(std::thread::hardware_concurrency(), 1u);
  while (!std::empty(nodes)) {
    using bfs_return_type = decltype(bfs(std::declval<T>(), nodes, edge_f));
    using bfs_future_type = std::future<bfs_return_type>;

    auto iterated_clusters = std::vector<bfs_future_type>{};
    auto node_it = std::begin(nodes);
    const auto end_it = std::end(nodes);
    for (auto i = decltype(max_threads){}; i < max_threads && node_it != end_it;
         ++i) {
      auto&& picked_node = *node_it;
      ++node_it;
      auto bfs_future =
          std::async(std::launch::async, [&nodes, &edge_f, &picked_node]() {
            return bfs(std::forward<decltype(picked_node)>(picked_node), nodes,
                       edge_f);
          });
      iterated_clusters.push_back(std::move(bfs_future));
    }

    std::for_each(std::cbegin(iterated_clusters), std::cend(iterated_clusters),
                  [](auto&& fut) { fut.wait(); });
    for (auto&& node_cluster_fut : iterated_clusters) {
      auto&& node_cluster = node_cluster_fut.get();
      // A cluster cannot be empty as it at least contains the picked node.
      assert(std::size(node_cluster) >= 1);
      // Due to symmetry of edge_f, a cluster might either be entirely contained
      // in nodes or not at all.
      auto&& sample_node = *std::begin(node_cluster);
      if (nodes.find(sample_node) != std::end(nodes)) {
        for (auto&& conn_node : node_cluster) {
#ifdef NDEBUG
          nodes.erase(conn_node);
#else
          assert(nodes.erase(conn_node));
#endif
        }
        *outit = std::move(node_cluster);
        ++outit;
      }
#ifndef NDEBUG
      else {
        for (auto&& conn_node : node_cluster) {
          assert(nodes.find(conn_node) == std::end(nodes));
        }
      }
#endif
    }
  }
  return outit;
}
} // namespace agglomerative
} // namespace flt::clustering

#endif
