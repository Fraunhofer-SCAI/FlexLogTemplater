#ifndef FLT_STRING_LEVENSHTEIN_DISTANCE_HPP
#define FLT_STRING_LEVENSHTEIN_DISTANCE_HPP

#include <flt/util/type_utils.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

namespace flt::string {
inline namespace distances {
namespace detail {
template <typename ForwardIt1, typename ForwardIt2, typename IterSize1,
          typename IterSize2, typename F>
[[nodiscard]] auto levenshtein_distance_ordered(const ForwardIt1 lhs_begin,
                                                const ForwardIt2 rhs_begin,
                                                const IterSize1 lhs_size,
                                                const IterSize2 rhs_size,
                                                F&& comp_fn) {
  static_assert(flt::util::is_forward_iterator<ForwardIt1>(),
                "ForwardIt1 must be a ForwardIterator");
  static_assert(flt::util::is_forward_iterator<ForwardIt2>(),
                "ForwardIt2 must be a ForwardIterator");

  using size_type = std::common_type_t<IterSize1, IterSize2>;
  static_assert(std::is_integral_v<size_type>);
  assert(lhs_size <= rhs_size);
  auto v = std::vector<size_type>(lhs_size + 1);

  std::iota(std::begin(v) + 1, std::end(v), size_type{1});

  for (auto [i, rhs_iter] = std::tuple{size_type{1}, rhs_begin}; i <= rhs_size;
       ++i, ++rhs_iter) {
    v[0] = i;

    for (auto [j, lhs_iter, last_diag] =
             std::tuple{size_type{1}, lhs_begin, i - 1};
         j <= lhs_size; ++j, ++lhs_iter) {
      const auto previous_diag = v[j];
      v[j] = std::min({
          v[j] + 1,     // Deletion cost
          v[j - 1] + 1, // Insertion cost
          last_diag +
              (comp_fn(*lhs_iter, *rhs_iter) ? 0 : 1) // Substitution cost
      });
      last_diag = std::move(previous_diag);
    }
  }

  return std::move(v.back());
}
} // namespace detail

template <typename ForwardIt1, typename ForwardIt2, typename F>
[[nodiscard]] inline auto
levenshtein_distance(ForwardIt1 lhs_begin, ForwardIt1 lhs_end,
                     ForwardIt2 rhs_begin, ForwardIt2 rhs_end, F&& comp_fn) {
  using size_type_lhs = std::make_unsigned_t<
      typename std::iterator_traits<ForwardIt1>::difference_type>;
  const auto lhs_size =
      static_cast<size_type_lhs>(std::distance(lhs_begin, lhs_end));
  using size_type_rhs = std::make_unsigned_t<
      typename std::iterator_traits<ForwardIt2>::difference_type>;
  const auto rhs_size =
      static_cast<size_type_rhs>(std::distance(rhs_begin, rhs_end));
  const auto lhs_shorter = (lhs_size <= rhs_size);
  if (lhs_shorter)
    return detail::levenshtein_distance_ordered(
        std::move(lhs_begin), std::move(rhs_begin), lhs_size, rhs_size,
        std::forward<F>(comp_fn));
  else
    return detail::levenshtein_distance_ordered(
        std::move(rhs_begin), std::move(lhs_begin), rhs_size, lhs_size,
        std::forward<F>(comp_fn));
}

template <typename TraitsT, typename ForwardIt1, typename ForwardIt2>
[[nodiscard]] inline auto
levenshtein_distance(ForwardIt1 lhs_begin, ForwardIt1 lhs_end,
                     ForwardIt2 rhs_begin, ForwardIt2 rhs_end) {
  const auto traits_eq = [](const auto& left, const auto& right) {
    return TraitsT::eq(left, right);
  };
  return levenshtein_distance(std::move(lhs_begin), std::move(lhs_end),
                              std::move(rhs_begin), std::move(rhs_end),
                              traits_eq);
}

template <typename U, typename V>
[[nodiscard]] inline auto levenshtein_distance(const U& lhs, const V& rhs) {
  using lhs_traits = typename std::decay_t<U>::traits_type;
  using rhs_traits = typename std::decay_t<V>::traits_type;
  static_assert(std::is_same_v<lhs_traits, rhs_traits>,
                "Incompatible traits passed");
  return levenshtein_distance<lhs_traits>(std::cbegin(lhs), std::cend(lhs),
                                          std::cbegin(rhs), std::cend(rhs));
}
} // namespace distances
} // namespace flt::string

#endif
