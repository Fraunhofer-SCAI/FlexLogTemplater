#ifndef FLT_STRING_LONGEST_COMMON_SUBSEQUENCE
#define FLT_STRING_LONGEST_COMMON_SUBSEQUENCE

#include <flt/string/string_table.hpp>
#include <flt/util/type_utils.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <numeric>
#include <optional>
#include <stack>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace flt::string {
inline namespace algorithms {
namespace lcs {
template <typename ValueType, typename IndexType> struct lcs_matrix_entry {
  ValueType value_;
  std::optional<IndexType> trace_;
};

template <typename ValueType,
          typename IndexType = std::make_unsigned_t<ValueType>>
using lcs_matrix = string_table::n_dimensional_string_table<
    lcs_matrix_entry<ValueType, IndexType>, IndexType>;

template <typename ForwardItIt, typename F>
[[nodiscard]] auto calculate_lcs_matrix(const ForwardItIt words_begin,
                                        const ForwardItIt words_end,
                                        F&& comp_fn) {
  static_assert(flt::util::is_forward_iterator<ForwardItIt>(),
                "ForwardItIt must be a ForwardIterator");
  using iter_pair_type = typename std::iterator_traits<ForwardItIt>::value_type;
  static_assert(flt::util::is_tuple<iter_pair_type>() &&
                    std::tuple_size_v<iter_pair_type> == 2u,
                "ForwardItIt must be a ForwardIterator over a 2-tuple of "
                "ForwardIterators");
  using inner_it_type = std::tuple_element_t<0, iter_pair_type>;
  static_assert(
      std::is_same_v<inner_it_type, std::tuple_element_t<1, iter_pair_type>>,
      "The tuple passed does not contain two identical types");
  static_assert(flt::util::is_forward_iterator<inner_it_type>(),
                "The tuple passed does not contain ForwardIterators");
  using inner_value_type =
      typename std::iterator_traits<inner_it_type>::value_type;
  static_assert(std::is_invocable_v<F, inner_value_type, inner_value_type>,
                "Cannot invoke comparison function with the given string type");
  using diff_type =
      typename std::iterator_traits<inner_it_type>::difference_type;
  using size_type = std::make_unsigned_t<diff_type>;

  const auto words_count =
      static_cast<size_type>(std::distance(words_begin, words_end));
  auto words_lengths = std::vector<size_type>();
  words_lengths.reserve(words_count);
  std::transform(words_begin, words_end, std::back_inserter(words_lengths),
                 [](auto&& it_pair) {
                   return static_cast<size_type>(std::distance(
                       std::get<0>(it_pair), std::get<1>(it_pair)));
                 });

  auto value_matrix = lcs_matrix<diff_type>{std::cbegin(words_lengths),
                                            std::cend(words_lengths)};

  const auto inner_dynamic_loop =
      [&](const auto i, auto i_iterator, auto&& iterator_vector,
          auto&& position_vector, auto&& rec_func) -> void {
    iterator_vector[i] = std::get<0>(*i_iterator);
    position_vector[i] = 1;
    for (; position_vector[i] <= words_lengths[i];
         ++position_vector[i], ++iterator_vector[i]) {
      if (i + 1 == words_count) {
        auto cur_index = value_matrix.calculate_offset(position_vector);
        auto& cur_entry = value_matrix[cur_index];

        auto last_word_match = false;
        {
          auto it_vec_it = std::cbegin(iterator_vector);
          last_word_match =
              std::all_of(it_vec_it + 1, std::cend(iterator_vector),
                          [&it_vec_it, &comp_fn](auto&& v) {
                            return comp_fn(*v, **it_vec_it);
                          });
        }

        if (last_word_match) {
          auto& previous_entry = value_matrix.at_all_relative(cur_index, -1);
          cur_entry.value_ = previous_entry.value_ + 1;
        } else {
          // The N-dimensional table is default-initialized during construction,
          // causing the values that should be initialized to 0 to be 0.
          auto& target_value = cur_entry.value_;
          for (auto cand_dim = size_type{0}; cand_dim < words_count;
               ++cand_dim) {
            const auto& cand_entry =
                value_matrix.at_relative(cur_index, cand_dim, -1);
            const auto cand_value = cand_entry.value_;
            if (target_value < cand_value) {
              target_value = cand_value;
              cur_entry.trace_ = cand_dim;
            }
          }
        }
      } else {
        rec_func(i + 1, std::next(i_iterator, 1), iterator_vector,
                 position_vector, rec_func);
      }
    }
  };

  auto iter_vector = std::vector<inner_it_type>(words_count);
  auto pos_vector = std::vector<size_type>(words_count);

  inner_dynamic_loop(size_type{0}, words_begin, iter_vector, pos_vector,
                     inner_dynamic_loop);

  return std::move(value_matrix);
}

template <typename InputIt, typename ValueType, typename SizeType>
[[nodiscard]] auto
find_lcs_path(const lcs_matrix<ValueType, SizeType>& lcs_mat,
              const InputIt first_string_it_begin,
              const InputIt first_string_it_end,
              const SizeType associated_index = SizeType{0}) {
  static_assert(flt::util::is_input_iterator<InputIt>(),
                "InputIt must be an InputIterator");
  using diff_type = typename std::iterator_traits<InputIt>::difference_type;
  const auto lcs_len = static_cast<SizeType>(lcs_mat.back().value_);

  auto lcs_marking_stack = std::stack<bool>();
  auto cur_index = lcs_mat.get_back_offset();
  for (auto res_counter = decltype(lcs_len){0}; res_counter != lcs_len;) {
    const auto& cur_entry = lcs_mat[cur_index];
    if (!cur_entry.trace_) {
      lcs_marking_stack.push(true);
      ++res_counter;
      const auto prev_index =
          lcs_mat.calculate_all_relative_offset(cur_index, -1);
      assert(lcs_mat[prev_index].value_ + 1 == cur_entry.value_);
      cur_index = prev_index;
    } else {
      const auto index_dec_dim = cur_entry.trace_.value();
      const auto prev_index =
          lcs_mat.calculate_relative_offset(cur_index, index_dec_dim, -1);

      assert(lcs_mat[prev_index].value_ == cur_entry.value_);
      cur_index = prev_index;
      if (index_dec_dim == associated_index) {
        lcs_marking_stack.push(false);
      }
    }
  }

  const auto discard_size = lcs_mat.get_nth_wordlength(associated_index) -
                            std::size(lcs_marking_stack);
  using StringValueType = typename std::iterator_traits<InputIt>::value_type;
  auto output_vector = std::vector<StringValueType>();
  output_vector.reserve(lcs_len);
  auto first_string_it =
      std::next(first_string_it_begin, static_cast<diff_type>(discard_size));
  for (auto i = 0; first_string_it != first_string_it_end;
       ++i, ++first_string_it) {
    if (lcs_marking_stack.top()) {
      output_vector.push_back(*first_string_it);
    }
    lcs_marking_stack.pop();
  }
  return std::move(output_vector);
}
} // namespace lcs

template <typename ForwardItIt, typename F>
[[nodiscard]] auto longest_common_subsequence(const ForwardItIt words_begin,
                                              const ForwardItIt words_end,
                                              F&& comp_fn) {
  auto&& lcs_mat = lcs::calculate_lcs_matrix(words_begin, words_end,
                                             std::forward<F>(comp_fn));
  return lcs::find_lcs_path(std::forward<decltype(lcs_mat)>(lcs_mat),
                            std::get<0>(*words_begin),
                            std::get<1>(*words_begin));
}
} // namespace algorithms
} // namespace flt::string

#endif
