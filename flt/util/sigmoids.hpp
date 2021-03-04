#ifndef FLT_UTILS_SIGMOIDS_HPP
#define FLT_UTILS_SIGMOIDS_HPP

#include <cmath>
#include <type_traits>
#include <utility>

namespace flt::util {
inline namespace sigmoids {
template <typename T, typename K = T>
[[nodiscard]] inline auto logistic_decrease(const T x, const K nu = K{0},
                                            const K scale = K{1}) {
  using B = std::common_type_t<K, T>;
  using C =
      typename std::conditional_t<std::is_unsigned<B>::value,
                                  std::make_signed<B>, std::decay<B>>::type;
  static_assert(std::is_arithmetic_v<C>);
  return C{1} / (C{1} + std::exp(static_cast<C>(scale) * static_cast<C>(x) -
                                 static_cast<C>(nu)));
}
} // namespace sigmoids
} // namespace flt::util

#endif
