#ifndef FLT_STRING_LEVENSHTEIN_PATH_HPP
#define FLT_STRING_LEVENSHTEIN_PATH_HPP

#include <flt/string/string_table.hpp>
#include <flt/util/type_utils.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <stack>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace flt::string {
inline namespace distances {

enum class levenshtein_edit_op { retention, deletion, insertion, substitution };

namespace levenshtein {

template <typename DiffType>
using levenshtein_matrix =
    string_table::fixed_dimensional_string_table<2, DiffType>;

template <typename ForwardIt1, typename ForwardIt2, typename F>
[[nodiscard]] auto calculate_levenshtein_matrix(const ForwardIt1 lhs_begin,
                                                const ForwardIt1 lhs_end,
                                                const ForwardIt2 rhs_begin,
                                                const ForwardIt2 rhs_end,
                                                F&& comp_fn) {
  static_assert(flt::util::is_forward_iterator<ForwardIt1>(),
                "ForwardIt1 must be a ForwardIterator");
  static_assert(flt::util::is_forward_iterator<ForwardIt2>(),
                "ForwardIt2 must be a ForwardIterator");

  using lhs_diff_type =
      typename std::iterator_traits<ForwardIt1>::difference_type;
  using rhs_diff_type =
      typename std::iterator_traits<ForwardIt2>::difference_type;
  using size_type =
      std::make_unsigned_t<std::common_type_t<lhs_diff_type, rhs_diff_type>>;
  const auto lhs_size =
      static_cast<size_type>(std::distance(lhs_begin, lhs_end));
  const auto rhs_size =
      static_cast<size_type>(std::distance(rhs_begin, rhs_end));

  auto levensh_mat = levenshtein_matrix<size_type>{lhs_size, rhs_size};

  for (auto i = size_type{1}; i <= lhs_size; ++i) {
    levensh_mat(i, 0) = i;
  }

  for (auto j = size_type{1}; j <= rhs_size; ++j) {
    levensh_mat(0, j) = j;
  }

  for (auto [i, lhs_iter] = std::tuple{size_type{1}, lhs_begin}; i <= lhs_size;
       ++i, ++lhs_iter) {
    for (auto [j, rhs_iter] = std::tuple{size_type{1}, rhs_begin};
         j <= rhs_size; ++j, ++rhs_iter) {
      levensh_mat(i, j) = std::min({
          levensh_mat(i - 1, j) + 1, // Deletion cost
          levensh_mat(i, j - 1) + 1, // Insertion cost
          levensh_mat(i - 1, j - 1) +
              (comp_fn(*lhs_iter, *rhs_iter) ? 0 : 1) // Substitution cost
      });
    }
  }

  return std::move(levensh_mat);
}

template <typename DiffType>
[[nodiscard]] auto
find_levenshtein_path(const levenshtein_matrix<DiffType>& levensh_mat) {
  auto res_stack = std::stack<levenshtein_edit_op>();
  auto [i, j] = std::tuple{levensh_mat.get_nth_wordlength(0),
                           levensh_mat.get_nth_wordlength(1)};
  while (i > 0 && j > 0) {
    auto min_cost = levensh_mat(i - 1, j - 1);
    auto min_item =
        (levensh_mat(i, j) == min_cost ? levenshtein_edit_op::retention
                                       : levenshtein_edit_op::substitution);
    if (levensh_mat(i - 1, j) < min_cost) {
      min_cost = levensh_mat(i - 1, j);
      min_item = levenshtein_edit_op::deletion;
    }
    if (levensh_mat(i, j - 1) < min_cost) {
      min_item = levenshtein_edit_op::insertion;
    }

    res_stack.push(min_item);
    switch (min_item) {
    case levenshtein_edit_op::retention:
      [[fallthrough]];
    case levenshtein_edit_op::substitution:
      --i;
      --j;
      break;
    case levenshtein_edit_op::deletion:
      --i;
      break;
    case levenshtein_edit_op::insertion:
      --j;
      break;
    }
  }
  const auto k = std::max(i, j);
  auto v_res = std::vector<levenshtein_edit_op>(k);
  v_res.reserve(k + std::size(res_stack));

  auto v_res_begin = std::begin(v_res);
  using diff_type = std::make_signed_t<
      typename std::iterator_traits<decltype(v_res_begin)>::difference_type>;
  if (i > 0) {
    std::fill(std::move(v_res_begin),
              std::next(v_res_begin, static_cast<diff_type>(i)),
              levenshtein_edit_op::deletion);
  } else if (j > 0) {
    std::fill(std::move(v_res_begin),
              std::next(v_res_begin, static_cast<diff_type>(j)),
              levenshtein_edit_op::insertion);
  }

  while (!std::empty(res_stack)) {
    v_res.push_back(res_stack.top());
    res_stack.pop();
  }

  return std::tuple{levensh_mat.back(), std::move(v_res)};
}
} // namespace levenshtein

template <typename ForwardIt1, typename ForwardIt2, typename F>
[[nodiscard]] auto levenshtein_path(ForwardIt1 lhs_begin, ForwardIt1 lhs_end,
                                    ForwardIt2 rhs_begin, ForwardIt2 rhs_end,
                                    F&& comp_fn) {
  auto&& levensh_matrix = levenshtein::calculate_levenshtein_matrix(
      std::move(lhs_begin), std::move(lhs_end), std::move(rhs_begin),
      std::move(rhs_end), std::forward<F>(comp_fn));
  return levenshtein::find_levenshtein_path(
      std::forward<decltype(levensh_matrix)>(levensh_matrix));
}

template <typename TraitsT, typename ForwardIt1, typename ForwardIt2>
[[nodiscard]] inline auto
levenshtein_path(ForwardIt1 lhs_begin, ForwardIt1 lhs_end, ForwardIt2 rhs_begin,
                 ForwardIt2 rhs_end) {
  const auto traits_eq = [](const auto& left, const auto& right) {
    return TraitsT::eq(left, right);
  };
  return levenshtein_path(std::move(lhs_begin), std::move(lhs_end),
                          std::move(rhs_begin), std::move(rhs_end), traits_eq);
}

template <typename U, typename V>
[[nodiscard]] inline auto levenshtein_path(const U& lhs, const V& rhs) {
  using lhs_traits = typename std::decay_t<U>::traits_type;
  using rhs_traits = typename std::decay_t<V>::traits_type;
  static_assert(std::is_same_v<lhs_traits, rhs_traits>,
                "Incompatible traits passed");
  return levenshtein_path<lhs_traits>(std::cbegin(lhs), std::cend(lhs),
                                      std::cbegin(rhs), std::cend(rhs));
}
} // namespace distances
} // namespace flt::string

#endif
