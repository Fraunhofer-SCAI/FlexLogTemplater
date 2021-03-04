#ifndef FLT_PARAMETER_FILTER_FILTER_ARRAY_HPP
#define FLT_PARAMETER_FILTER_FILTER_ARRAY_HPP

#include <algorithm>
#include <forward_list>
#include <optional>
#include <utility>

namespace flt::parameter_filter {
template <typename OutIt, typename T> class filter_array {
protected:
  struct filter_base {
    virtual ~filter_base() = default;
    virtual T operator()(const T& line_str, OutIt outit) const = 0;
    virtual T operator()(const T& line_str) const = 0;
  };
  template <typename FilterT>
  struct filter_abstracter final : public filter_base {
    filter_abstracter(FilterT filter) : filter_(std::move(filter)) {}
    const FilterT filter_;

    T operator()(const T& line_str, OutIt outit) const override {
      return filter_(line_str, outit);
    }

    T operator()(const T& line_str) const override { return filter_(line_str); }
  };

  using ptr_type = std::unique_ptr<filter_base>;

public:
  filter_array() = default;
  template <typename... FilterT> filter_array(FilterT&&... filts) {
    add_filter(std::forward<decltype(filts)>(filts)...);
  }

  auto operator()(const T& line_str, OutIt outit) const {
    auto res_str = line_str;
    for (auto&& filt : filt_array_) {
      res_str = (*filt)(res_str, outit);
    }
    return res_str;
  }

  auto operator()(const T& line_str) const {
    auto res_str = line_str;
    for (auto&& filt : filt_array_) {
      res_str = (*filt)(res_str);
    }
    return res_str;
  }

  template <typename FilterT> void add_filter(FilterT&& filt) {
    using filter_ptr_type = filter_abstracter<std::decay_t<FilterT>>;
    auto myptr = std::make_unique<filter_ptr_type>(std::forward<FilterT>(filt));
    filt_array_.emplace_front(std::move(myptr));
  }

  template <typename FilterT, typename... FilterTypes>
  void add_filter(FilterT&& filt, FilterTypes&&... other_filts) {
    add_filter(std::forward<FilterT>(filt));
    add_filter(std::forward<FilterTypes>(other_filts)...);
  }

protected:
  std::forward_list<ptr_type> filt_array_;
};
} // namespace flt::parameter_filter

#endif
