#ifndef FLT_STRING_WEIGHTED_EDIT_DISTANCE_HPP
#define FLT_STRING_WEIGHTED_EDIT_DISTANCE_HPP

#include <flt/util/type_utils.hpp>

#include <type_traits>

namespace flt::string {
inline namespace distances {
template <typename InputIterator1, typename InputIterator2, typename F,
          typename W>
[[nodiscard]] auto weighted_edit_distance(
    InputIterator1 first_string_begin, const InputIterator1 first_string_end,
    InputIterator2 second_string_begin, const InputIterator2 second_string_end,
    F&& distance_measure, W&& weight_function) {
  static_assert(flt::util::is_input_iterator<InputIterator1>(),
                "InputIterator1 is not an InputIterator");
  static_assert(flt::util::is_input_iterator<InputIterator2>(),
                "InputIterator2 is not an InputIterator");

  using size_type = std::make_unsigned_t<std::common_type_t<
      typename std::iterator_traits<InputIterator1>::difference_type,
      typename std::iterator_traits<InputIterator2>::difference_type>>;
  static_assert(std::is_invocable_v<W, size_type>,
                "Cannot invoke weight function with given size type");
  using weight_type = std::decay_t<std::invoke_result_t<W, size_type>>;
  static_assert(std::is_arithmetic_v<weight_type>,
                "Weight function does not return an arithmetic type");

  using first_value_type =
      typename std::iterator_traits<InputIterator1>::value_type;
  using second_value_type =
      typename std::iterator_traits<InputIterator2>::value_type;

  static_assert(std::is_default_constructible_v<first_value_type>,
                "InputIterator1 has a non-default constructible value type");
  static_assert(std::is_default_constructible_v<second_value_type>,
                "InputIterator2 has a non-default constructible value type");
  static_assert(std::is_invocable_v<F, first_value_type, second_value_type>,
                "Cannot invoke distance function with given iterators");

  auto weighted_distance = weight_type{0};
  auto word_counter = size_type{0};

  for (; (first_string_begin != first_string_end &&
          second_string_begin != second_string_end);) {
    weighted_distance +=
        distance_measure(*first_string_begin, *second_string_begin) *
        weight_function(++word_counter);
    ++first_string_begin;
    ++second_string_begin;
  }

  for (const auto empty_second = second_value_type{};
       first_string_begin != first_string_end; ++first_string_begin) {
    weighted_distance += distance_measure(*first_string_begin, empty_second) *
                         weight_function(++word_counter);
  }

  for (const auto empty_first = first_value_type{};
       second_string_begin != second_string_end; ++second_string_begin) {
    weighted_distance += distance_measure(empty_first, *second_string_begin) *
                         weight_function(++word_counter);
  }

  return weighted_distance;
}
} // namespace distances
} // namespace flt::string

#endif
