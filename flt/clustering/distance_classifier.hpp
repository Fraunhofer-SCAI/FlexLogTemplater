#ifndef FLT_CLUSTERING_DISTANCE_CLASSIFIER_HPP
#define FLT_CLUSTERING_DISTANCE_CLASSIFIER_HPP

#include <flt/util/type_utils.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iterator>
#include <numeric>
#include <type_traits>
#include <utility>
#ifndef NDEBUG
#include <optional>
#endif

namespace flt::clustering {
inline namespace distance_classifier {
template <typename ForwardIt>
auto distance_classification_threshold_sorted(const ForwardIt first,
                                              const ForwardIt last) {
  static_assert(flt::util::is_forward_iterator<ForwardIt>(),
                "ForwardIt must be a forward iterator");
  using ValueType = typename std::iterator_traits<ForwardIt>::value_type;
  static_assert(std::is_arithmetic_v<ValueType>,
                "Value type of ForwardIt must be arithmetic");
  // If the ValueType is integral, float should be more than sufficient to
  // determine a seperation point.
  using ResultType =
      std::conditional_t<std::is_floating_point_v<ValueType>, ValueType, float>;

  // Since the range is sorted, two starting means can be estimated as the
  // average of the first half of the values and upper half of them,
  // respectively.
  const auto num_values = std::distance(first, last);

  if (num_values < 2)
    throw std::invalid_argument{
        "Cannot classify a set with less than 2 distances!"};

  const auto mid_range_it = std::next(first, num_values / 2);
  auto upper_bound_it = std::lower_bound(first, mid_range_it, *mid_range_it);
  if (upper_bound_it == first) {
    upper_bound_it = std::upper_bound(mid_range_it, last, *mid_range_it);
    // If all values are identical we will arrive here, and this situation
    // cannot be classified.
    if (upper_bound_it == last) {
      throw std::invalid_argument{"Cannot classify a set of identical values!"};
    }
  }
  auto last_cutoff_value = ResultType{};
  if constexpr (flt::util::is_bidirectional_iterator<ForwardIt>()) {
    last_cutoff_value = static_cast<ResultType>(*std::prev(upper_bound_it));
  } else {
    const auto upper_bound_dist = std::distance(first, upper_bound_it);
    last_cutoff_value = static_cast<ResultType>(*std::next(first, upper_bound_dist - 1));
  }
  auto num_inner_means = std::distance(first, upper_bound_it);
  auto num_inter_means = num_values - num_inner_means;

#if defined(__cpp_lib_parallel_algorithm) &&                                   \
    __cpp_lib_parallel_algorithm >= 201603
  auto sum_inner_means = std::reduce(first, upper_bound_it, ResultType{});
  auto sum_inter_means = std::reduce(upper_bound_it, last, ResultType{});
#else
  auto sum_inner_means = std::accumulate(first, upper_bound_it, ResultType{});
  auto sum_inter_means = std::accumulate(upper_bound_it, last, ResultType{});
#endif

#ifndef NDEBUG
  auto underest_case = std::optional<bool>{};
#endif

  for (auto assignments_changed = true; assignments_changed;) {
    const auto inner_means = sum_inner_means / num_inner_means;
    const auto inter_means = sum_inter_means / num_inter_means;
    const auto cutoff_value = (inner_means + inter_means) / 2;

    // We consider a value that falls on the cutoff to belong to the inner class
    // distance set.
    auto new_upper_bound_it = std::upper_bound(first, last, cutoff_value);
    // If new_upper_bound_it was first, then the cutoff_value has to be wrong.
    assert(new_upper_bound_it != first);
    // If the upper set becomes empty, then no reasonable cutoff can be
    // constructed. This is only possible if a range of identical values is
    // passed to the algorithm, which however is being caught during the
    // initialization.
    assert(new_upper_bound_it != last);
    assignments_changed = (new_upper_bound_it != upper_bound_it);
    if (assignments_changed) {
      assert(std::abs(cutoff_value - last_cutoff_value) >
             10 * std::numeric_limits<ResultType>::epsilon());
      const auto underest = cutoff_value > last_cutoff_value;
#ifndef NDEBUG
      if (underest_case.has_value()) {
        assert(underest_case.value() == underest);
      } else {
        underest_case = underest;
      }
#endif
      if (underest) {
        // The interval [upper_bound_it, new_upper_bound_it) becomes part of the
        // lower range
        std::for_each(upper_bound_it, new_upper_bound_it, [&](auto&& v) {
          sum_inner_means += v;
          ++num_inner_means;
          sum_inter_means -= v;
          --num_inter_means;
        });
      } else {
        // The interval [new_upper_bound_it, upper_bound_it) becomes part of the
        // higher range
        std::for_each(new_upper_bound_it, upper_bound_it, [&](auto&& v) {
          sum_inner_means -= v;
          --num_inner_means;
          sum_inter_means += v;
          ++num_inter_means;
        });
      }
      upper_bound_it = std::move(new_upper_bound_it);
      last_cutoff_value = cutoff_value;
    }
  }
  if constexpr (flt::util::is_bidirectional_iterator<ForwardIt>()) {
    return *std::prev(upper_bound_it);
  } else {
    const auto upper_bound_dist = std::distance(first, upper_bound_it);
    return *std::next(first, upper_bound_dist - 1);
  }
}

template <typename T>
auto distance_classification_threshold(T&& distance_values) {
  static_assert(flt::util::is_random_access_iterator<
                    typename std::decay_t<T>::iterator>(),
                "This function requires RandomAccessIterator begin");
  std::sort(std::begin(distance_values), std::end(distance_values));
  return distance_classification_threshold_sorted(std::cbegin(distance_values),
                                                  std::cend(distance_values));
}
} // namespace distance_classifier
} // namespace flt::clustering

#endif
