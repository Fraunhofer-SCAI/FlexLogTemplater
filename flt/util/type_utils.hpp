#ifndef FLT_TYPE_UTILS_HPP
#define FLT_TYPE_UTILS_HPP

#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>

namespace flt::util {
inline namespace type_utils {
namespace detail {
template <typename T, typename IterTag, typename = void>
struct is_tagged_iterator : public std::false_type {};

template <typename T, typename IterTag>
struct is_tagged_iterator<
    T, IterTag,
    std::enable_if_t<std::is_base_of_v<
        IterTag, typename std::iterator_traits<T>::iterator_category>>>
    : public std::true_type {};
} // namespace detail

template <typename T>
using is_input_iterator =
    detail::is_tagged_iterator<T, std::input_iterator_tag>;

template <typename T>
using is_output_iterator =
    detail::is_tagged_iterator<T, std::output_iterator_tag>;

template <typename T>
using is_forward_iterator =
    detail::is_tagged_iterator<T, std::forward_iterator_tag>;

template <typename T>
using is_bidirectional_iterator =
    detail::is_tagged_iterator<T, std::bidirectional_iterator_tag>;

template <typename T>
using is_random_access_iterator =
    detail::is_tagged_iterator<T, std::random_access_iterator_tag>;

template <typename T, typename = void>
struct is_pair : public std::false_type {};

template <typename T>
struct is_pair<T, std::void_t<decltype(std::declval<T>().first),
                              decltype(std::declval<T>().second)>>
    : public std::true_type {};

template <typename T, typename = void>
struct is_tuple : public std::false_type {};

template <typename T>
struct is_tuple<T, std::void_t<decltype(std::tuple_size<T>::value)>>
    : public std::true_type {};

} // namespace type_utils
} // namespace flt::util

#endif
