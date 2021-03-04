#ifndef FLT_STRING_DYNAMIC_STRING_TABLE
#define FLT_STRING_DYNAMIC_STRING_TABLE

#include <flt/util/type_utils.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <type_traits>
#include <vector>

namespace flt::string::string_table {
namespace detail {
template <typename IndexVectorType, typename T> class basic_string_table {
protected:
  basic_string_table() = default;

public:
  using index_vector_type = IndexVectorType;
  using index_scalar_type = typename index_vector_type::size_type;
  using index_relative_type = std::make_signed_t<index_scalar_type>;

  inline auto begin() noexcept { return std::begin(value_matrix_); }
  inline auto end() noexcept { return std::end(value_matrix_); }
  inline auto cbegin() const noexcept { return std::cbegin(value_matrix_); }
  inline auto cend() const noexcept { return std::cend(value_matrix_); }
  inline auto size() const noexcept { return std::size(value_matrix_); }
  inline auto data() noexcept { return std::data(value_matrix_); }
  inline auto data() const noexcept { return std::data(value_matrix_); }

  inline auto& back() noexcept { return value_matrix_.back(); }
  inline const auto& back() const noexcept { return value_matrix_.back(); }
  inline auto& front() noexcept { return value_matrix_.front(); }
  inline const auto& front() const noexcept { return value_matrix_.front(); }

  inline auto& operator[](const index_vector_type& index_v) noexcept {
    return value_matrix_[calculate_offset(index_v)];
  }
  inline const auto& operator[](const index_vector_type& index_v) const
      noexcept {
    return value_matrix_[calculate_offset(index_v)];
  }
  inline auto& operator[](const index_scalar_type& index_s) noexcept {
    assert(index_s < std::size(value_matrix_));
    return value_matrix_[index_s];
  }
  inline const auto& operator[](const index_scalar_type& index_s) const
      noexcept {
    assert(index_s < std::size(value_matrix_));
    return value_matrix_[index_s];
  }
  inline auto& at_relative(const index_scalar_type index_s,
                           const index_scalar_type index_r,
                           const index_relative_type index_r_value) noexcept {
    return value_matrix_[calculate_relative_offset(index_s, index_r,
                                                   index_r_value)];
  }
  inline const auto& at_relative(const index_scalar_type index_s,
                                 const index_scalar_type index_r,
                                 const index_relative_type index_r_value) const
      noexcept {
    return value_matrix_[calculate_relative_offset(index_s, index_r,
                                                   index_r_value)];
  }
  inline auto&
  at_all_relative(const index_scalar_type index_s,
                  const index_relative_type index_r_value) noexcept {
    return value_matrix_[calculate_all_relative_offset(index_s, index_r_value)];
  }
  inline const auto&
  at_all_relative(const index_scalar_type index_s,
                  const index_relative_type index_r_value) const noexcept {
    return value_matrix_[calculate_all_relative_offset(index_s, index_r_value)];
  }

  inline auto calculate_offset(const index_vector_type& index_v) const
      noexcept {
    assert(std::size(index_v) == std::size(index_offsets_));
    const auto index_s = std::inner_product(
        std::next(std::cbegin(index_v), 1), std::cend(index_v),
        std::next(std::cbegin(index_offsets_), 1), index_v[0]);
    assert(index_s < std::size(value_matrix_));
    return index_s;
  }
  inline auto calculate_relative_offset(
      const index_scalar_type index_s, const index_scalar_type index_r,
      const index_relative_type index_r_value) const noexcept {
    const auto index_rs =
        adjust_by_relative(index_s, index_r_value, index_offsets_[index_r]);
    assert(index_rs < std::size(value_matrix_));
    return index_rs;
  }
  inline auto
  calculate_all_relative_offset(const index_scalar_type index_s,
                                const index_relative_type index_r_value) const
      noexcept {
    auto index_ra = index_s;
    std::for_each(std::cbegin(index_offsets_), std::cend(index_offsets_),
                  [&index_ra, index_r_value, this](auto offval) {
                    index_ra =
                        adjust_by_relative(index_ra, index_r_value, offval);
                  });
    assert(index_ra < std::size(value_matrix_));
    return index_ra;
  }
  inline auto get_back_offset() const noexcept {
    return std::size(value_matrix_) - 1;
  }
  inline const auto& get_index_offsets() const noexcept {
    return index_offsets_;
  }
  inline auto get_dimension() const noexcept {
    return std::size(index_offsets_);
  }
  inline auto get_nth_wordlength(const index_scalar_type queried_dim) const
      noexcept {
    assert(queried_dim < std::size(index_offsets_));
    const auto lenp1 = ((queried_dim == std::size(index_offsets_) - 1)
                            ? std::size(value_matrix_)
                            : index_offsets_[queried_dim + 1]) /
                       index_offsets_[queried_dim];
    return lenp1 - 1;
  }

protected:
  inline auto adjust_by_relative(const index_scalar_type index_s,
                                 const index_relative_type index_r_value,
                                 const index_scalar_type index_off) const
      noexcept {
    if (index_r_value < 0)
      return index_s -
             static_cast<index_scalar_type>(-index_r_value) * index_off;
    else
      return index_s +
             static_cast<index_scalar_type>(index_r_value) * index_off;
  }

  std::vector<T> value_matrix_;
  index_vector_type index_offsets_;
};
} // namespace detail

template <typename T, typename SizeType = std::size_t>
class n_dimensional_string_table
    : public detail::basic_string_table<std::vector<SizeType>, T> {
public:
  template <typename InputIt>
  n_dimensional_string_table(InputIt it_first, InputIt it_last) {
    static_assert(flt::util::is_input_iterator<InputIt>(),
                  "InputIt must be an InputIterator");
    using input_type = typename std::iterator_traits<InputIt>::value_type;
    static_assert(std::is_integral_v<input_type>,
                  "Input range must be a range of integral extents");
    auto offset_multiplier = SizeType{1};
    std::transform(it_first, it_last, std::back_inserter(this->index_offsets_),
                   [&offset_multiplier](auto&& len) {
                     const auto old_multiplier = offset_multiplier;
                     offset_multiplier *= (len + 1);
                     return old_multiplier;
                   });
    this->value_matrix_.resize(offset_multiplier);
  }
};

template <auto N, typename T, typename SizeType = std::size_t>
class fixed_dimensional_string_table
    : public detail::basic_string_table<std::array<SizeType, N>, T> {
public:
  template <typename... Dims>
  fixed_dimensional_string_table(Dims... dimensions) {
    static_assert(sizeof...(Dims) == N,
                  "Constructor for size N needs N dimension arguments.");
    const auto total_size =
        fill_offset_table(std::begin(this->index_offsets_), SizeType{1},
                          static_cast<SizeType>(dimensions)...);
    this->value_matrix_.resize(total_size);
  }

  template <typename... Indices> inline auto& operator()(Indices... indices) {
    return this->value_matrix_[calculate_offset(indices...)];
  }
  template <typename... Indices>
  inline const auto& operator()(Indices... indices) const {
    return this->value_matrix_[calculate_offset(indices...)];
  }

  template <typename... Indices>
  inline auto calculate_offset(Indices... indices) const noexcept {
    static_assert(sizeof...(Indices) == N,
                  "Constructor for size N needs N dimension arguments.");
    return impl_calculate_offset(std::cbegin(this->index_offsets_),
                                 static_cast<SizeType>(indices)...);
  }

protected:
  template <typename OutIt, typename... DimsR>
  inline auto fill_offset_table(OutIt target_slot, SizeType current_size,
                                SizeType target_dim, DimsR... remaining_dims) {
    *target_slot = current_size;
    return fill_offset_table(std::move(++target_slot),
                             current_size * (target_dim + 1),
                             remaining_dims...);
  }

  template <typename OutIt>
  inline auto fill_offset_table(OutIt target_slot, SizeType current_size,
                                SizeType target_dim) {
    *target_slot = current_size;
    return current_size * (target_dim + 1);
  }

  template <typename InputIt, typename... Indices>
  inline auto impl_calculate_offset(InputIt offset_it, SizeType cur_index,
                                    Indices... indices) const noexcept {
    const auto index_res = *offset_it * cur_index;
    return index_res +
           impl_calculate_offset(std::move(++offset_it), indices...);
  }

  template <typename InputIt>
  inline auto impl_calculate_offset(InputIt offset_it, SizeType cur_index) const
      noexcept {
    return *offset_it * cur_index;
  }
};

} // namespace flt::string::string_table

#endif
