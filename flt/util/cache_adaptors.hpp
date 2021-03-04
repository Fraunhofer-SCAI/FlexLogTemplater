#ifndef FLT_UTILS_CACHE_ADAPTORS_HPP
#define FLT_UTILS_CACHE_ADAPTORS_HPP

#include <flt/util/hash_combine.hpp>

#include <functional>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace flt::util {
inline namespace caches {
namespace detail {
template <typename Hash, typename KeyEqual, typename F, typename... Params>
class cache_adaptor {
public:
  using value_type = std::invoke_result_t<F, Params...>;

  cache_adaptor(F bound_f) : bound_f_{std::move(bound_f)} {}

  auto operator()(const Params&... pars) {
    auto&& map_key = std::make_tuple(pars...);
    if (auto&& it = result_cache_.find(map_key); it != result_cache_.end()) {
      return it->second;
    }
    auto&& result_val = bound_f_(pars...);
    result_cache_.emplace(std::move(map_key), result_val);
    return result_val;
  }

protected:
  F bound_f_;
  std::unordered_map<std::tuple<Params...>, value_type, Hash, KeyEqual>
      result_cache_;
};

template <typename Hash, typename KeyEqual, typename F, typename... Params>
class synchronized_cache_adaptor {
public:
  using value_type = std::invoke_result_t<F, Params...>;

  synchronized_cache_adaptor(F bound_f) : bound_f_{std::move(bound_f)} {}

  auto operator()(const Params&... pars) {
    auto&& map_key = std::make_tuple(pars...);
    {
      auto read_lock = std::shared_lock{cache_mtx_};
      if (auto&& it = result_cache_.find(map_key); it != result_cache_.end()) {
        return it->second;
      }
    }
    {
      std::lock_guard write_lock{cache_mtx_};
      if (auto&& it = result_cache_.find(map_key); it != result_cache_.end()) {
        return it->second;
      }
      auto&& result_val = bound_f_(pars...);
      result_cache_.emplace(std::move(map_key), result_val);
      return result_val;
    }
  }

protected:
  F bound_f_;
  std::unordered_map<std::tuple<Params...>, value_type, Hash, KeyEqual>
      result_cache_;
  std::shared_mutex cache_mtx_;
};

template <typename U, typename V> class symmetric_tuple_compare {
public:
  constexpr auto operator()(const std::tuple<U, V>& tupl,
                            const std::tuple<U, V>& other) const {
    return (std::get<0>(tupl) == std::get<0>(other) &&
            std::get<1>(tupl) == std::get<1>(other)) ||
           (std::get<1>(tupl) == std::get<0>(other) &&
            std::get<0>(tupl) == std::get<1>(other));
  }
};
} // namespace detail

template <typename F, typename... Params>
using ordered_cache_adaptor =
    detail::cache_adaptor<ordered_tuple_hash,
                          std::equal_to<std::tuple<Params...>>, F, Params...>;

template <typename F, typename... Params>
using symmetric_cache_adaptor =
    detail::cache_adaptor<symmetric_tuple_hash,
                          detail::symmetric_tuple_compare<Params...>, F,
                          Params...>;

template <typename F, typename... Params>
using ordered_sync_cache_adaptor = detail::synchronized_cache_adaptor<
    ordered_tuple_hash, std::equal_to<std::tuple<Params...>>, F, Params...>;

template <typename F, typename... Params>
using symmetric_sync_cache_adaptor = detail::synchronized_cache_adaptor<
    symmetric_tuple_hash, detail::symmetric_tuple_compare<Params...>, F,
    Params...>;
} // namespace caches
} // namespace flt::util

#endif
