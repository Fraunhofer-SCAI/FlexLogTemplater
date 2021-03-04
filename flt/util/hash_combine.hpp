#ifndef FLT_UTILS_HASH_COMBINE_HPP
#define FLT_UTILS_HASH_COMBINE_HPP

#include <functional>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace flt::util {
inline namespace hashes {
namespace detail {
template <typename T> constexpr auto hex_phi = T{0x0.9e3779b97f4a8p+0};

template <typename T>
constexpr auto hash_constant = T{static_cast<T>(
    (T{1} << (std::numeric_limits<T>::digits - 1)) * hex_phi<double>)};
} // namespace detail

template <typename T> constexpr auto hash_combine(T left, T right) {
  return left ^ (right + detail::hash_constant<T> +
                 (left << (std::numeric_limits<T>::digits - 2)) + (left >> 2));
}

template <typename T, typename... Targs>
constexpr auto hash_combine(T left, T right, Targs... otherargs) {
  return hash_combine(left, hash_combine(right, otherargs...));
}

template <typename T, typename = std::enable_if_t<
                          std::is_default_constructible_v<std::hash<T>>>>
constexpr auto multi_hash(const T& left) {
  return std::hash<T>{}(left);
}

template <typename T, typename... Targs,
          typename = std::enable_if_t<(sizeof...(Targs) > 0)>>
constexpr auto multi_hash(const T& left, Targs... otherargs) {
  return hash_combine(multi_hash(left), multi_hash(otherargs...));
}

namespace detail {
template <typename Tuple, typename I, auto... Is>
constexpr auto multi_hash_impl(Tuple&& tupl, std::integer_sequence<I, Is...>) {
  return multi_hash(std::get<Is>(std::forward<Tuple>(tupl))...);
}
} // namespace detail

template <typename Tuple,
          typename = std::enable_if_t<
              !std::is_default_constructible_v<std::hash<Tuple>>>,
          typename TuplSize = std::tuple_size<std::decay_t<Tuple>>,
          typename = std::enable_if_t<(TuplSize::value > 1)>,
          typename Inds = std::make_index_sequence<TuplSize::value>>
constexpr auto multi_hash(Tuple&& tupl) {
  return detail::multi_hash_impl(std::forward<Tuple>(tupl), Inds{});
}

template <typename T> constexpr auto symmetric_hash_combine(T left, T right) {
  if (right < left) {
    return hash_combine(right, left);
  } else {
    return hash_combine(left, right);
  }
}

template <typename T, typename = std::enable_if_t<
                          std::is_default_constructible_v<std::hash<T>>>>
constexpr auto symmetric_multi_hash(const T& left) {
  return std::hash<T>{}(left);
}

template <typename T>
constexpr auto symmetric_multi_hash(const T& left, const T& right) {
  return symmetric_hash_combine(symmetric_multi_hash(left),
                                symmetric_multi_hash(right));
}

namespace detail {
template <typename Tuple, typename I, auto... Is>
constexpr auto symmetric_multi_hash_impl(Tuple&& tupl,
                                         std::integer_sequence<I, Is...>) {
  return symmetric_multi_hash(std::get<Is>(std::forward<Tuple>(tupl))...);
}
} // namespace detail

template <typename Tuple,
          typename = std::enable_if_t<
              !std::is_default_constructible_v<std::hash<Tuple>>>,
          typename TuplSize = std::tuple_size<std::decay_t<Tuple>>,
          typename = std::enable_if_t<(TuplSize::value > 1)>,
          typename Inds = std::make_index_sequence<TuplSize::value>>
constexpr auto symmetric_multi_hash(Tuple&& tupl) {
  return detail::symmetric_multi_hash_impl(std::forward<Tuple>(tupl), Inds{});
}

class ordered_tuple_hash {
public:
  template <typename T> constexpr auto operator()(T&& tupl) const {
    return multi_hash(std::forward<T>(tupl));
  }
};

class symmetric_tuple_hash {
public:
  template <typename T> constexpr auto operator()(T&& tupl) const {
    return symmetric_multi_hash(std::forward<T>(tupl));
  }
};
} // namespace hashes
} // namespace flt::util

#endif
