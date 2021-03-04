#ifndef FLT_STRING_WORD_ITERATOR_HPP
#define FLT_STRING_WORD_ITERATOR_HPP

#include <algorithm>
#include <cassert>
#include <iterator>
#include <locale>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace flt::string {
inline namespace iterator {
template <typename U> class word_iterator {
protected:
  using string_iterator_type = typename U::const_iterator;
  using char_type = typename U::value_type;
  using facet_type = std::ctype<char_type>;
  using traits_type = typename U::traits_type;

public:
  using difference_type =
      typename std::iterator_traits<string_iterator_type>::difference_type;
  using value_type = std::basic_string_view<char_type, traits_type>;
  using pointer = std::add_pointer_t<value_type>;
  using reference = std::add_lvalue_reference_t<value_type>;
  using iterator_category = std::forward_iterator_tag;

  word_iterator(string_iterator_type string_begin,
                string_iterator_type string_end,
                std::locale loc = std::locale(),
                std::ctype_base::mask mask = std::ctype_base::blank)
      : word_begin_{std::move(string_begin)}, prev_word_end_{word_begin_},
        string_end_{std::move(string_end)}, loc_{std::move(loc)},
        mask_{std::move(mask)} {
    if (word_begin_ != string_end)
      std::advance(word_begin_, find_distance_to_next_word(word_begin_));
    construct_view();
  }

  word_iterator() = default;
  word_iterator(const word_iterator<U>&) = default;
  word_iterator(word_iterator<U>&&) = default;
  word_iterator<U>& operator=(const word_iterator<U>&) = default;
  word_iterator<U>& operator=(word_iterator<U>&&) = default;

  virtual ~word_iterator() = default;

  auto& operator*() { return word_string_view_; }

  auto* operator-> () { return &word_string_view_; }

  const auto& operator*() const { return word_string_view_; }

  const auto* operator-> () const { return &word_string_view_; }

  auto operator==(const word_iterator<U>& other) const {
    return (this->word_begin_ == other.word_begin_);
  }

  auto operator!=(const word_iterator<U>& other) const {
    return !(*this == other);
  }

  auto& operator++() {
    assert(std::size(word_string_view_));
    auto&& word_end_it = word_begin_;
    std::advance(word_end_it,
                 static_cast<difference_type>(std::size(word_string_view_)));
    prev_word_end_ = word_end_it;
    if (word_end_it == string_end_) {
      word_begin_ = std::forward<decltype(word_end_it)>(word_end_it);
    } else {
      std::advance(word_begin_, find_distance_to_next_word(word_begin_));
    }
    construct_view();
    return *this;
  }

  auto operator++(int) {
    auto it_copy = *this;
    ++*this;
    return std::move(it_copy);
  }

  auto get_iterators() const {
    const auto word_size = std::size(word_string_view_);
    auto end_iterator = std::next(word_begin_, word_size);
    return std::tuple{word_begin_, std::move(end_iterator)};
  }

  auto get_iterators() {
    const auto word_size = std::size(word_string_view_);
    auto end_iterator = std::next(word_begin_, word_size);
    return std::tuple{word_begin_, std::move(end_iterator)};
  }

  auto get_prev_seps() {
    return value_type{&*prev_word_end_,
                      static_cast<typename value_type::size_type>(
                          std::distance(&*prev_word_end_, &*word_begin_))};
  }

  auto get_next_seps() {
    auto word_end_it = word_begin_;
    std::advance(word_end_it,
                 static_cast<difference_type>(std::size(word_string_view_)));
    if (word_end_it == string_end_) {
      return value_type{};
    } else {
      auto next_word_it = word_end_it;
      std::advance(next_word_it, find_distance_to_next_word(word_end_it));
      return value_type{&*word_end_it,
                        static_cast<typename value_type::size_type>(
                            std::distance(&*word_end_it, &*next_word_it))};
    }
  }

protected:
  virtual typename std::iterator_traits<
      std::add_pointer_t<char_type>>::difference_type
  find_current_word_length() {
    auto&& word_end = std::use_facet<facet_type>(loc_).scan_is(
        mask_, &*word_begin_, endptr());
    return std::distance(&*word_begin_, word_end);
  }

  virtual typename std::iterator_traits<
      std::add_pointer_t<char_type>>::difference_type
  find_distance_to_next_word(string_iterator_type from) {
    auto&& next_word =
        std::use_facet<facet_type>(loc_).scan_not(mask_, &*from, endptr());
    return std::distance(&*from, next_word);
  }

  void construct_view() {
    if (word_begin_ == string_end_) {
      word_string_view_ =
          value_type{endptr(), typename value_type::size_type{0}};
    } else {
      const auto word_size = find_current_word_length();
      assert(word_size >= 0);
      word_string_view_ =
          value_type{&*word_begin_,
                     static_cast<typename value_type::size_type>(word_size)};
    }
  }

  constexpr inline auto endptr() { return &*(string_end_ - 1) + 1; }

  string_iterator_type word_begin_;
  string_iterator_type prev_word_end_;
  string_iterator_type string_end_;
  std::locale loc_;
  std::ctype_base::mask mask_;
  value_type word_string_view_;
};

template <typename U> class word_iterator_spec_seps : public word_iterator<U> {
public:
  using word_iterator<U>::word_iterator;
  using string_iterator_type = typename U::const_iterator;
  using char_type = typename U::value_type;

  word_iterator_spec_seps(string_iterator_type string_begin,
                          string_iterator_type string_end,
                          std::locale loc = std::locale(),
                          std::ctype_base::mask mask = std::ctype_base::blank,
                          U separators = "=;,'\"()[]{}")
      : word_iterator<U>(string_begin, string_end, loc, mask),
        separators_{std::move(separators)} {
    if (this->word_begin_ != string_end)
      std::advance(this->word_begin_,
                   find_distance_to_next_word(this->word_begin_));
    this->construct_view();
  }

protected:
  typename std::iterator_traits<std::add_pointer_t<char_type>>::difference_type
  find_current_word_length() final {
    auto word_end = &*this->word_begin_;
    while (word_end != this->endptr() && !std::isspace(*word_end, this->loc_) &&
           std::all_of(separators_.cbegin(), separators_.cend(),
                       [&](char sep) { return *word_end != sep; })) {
      ++word_end;
    }
    return std::distance(&*this->word_begin_, word_end);
  }

  typename std::iterator_traits<std::add_pointer_t<char_type>>::difference_type
  find_distance_to_next_word(string_iterator_type from) final {
    auto word_end = &*from;
    while (word_end != this->endptr() &&
           (std::isspace(*word_end, this->loc_) ||
            std::any_of(separators_.cbegin(), separators_.cend(),
                        [&](char sep) { return *word_end == sep; }))) {
      ++word_end;
    }
    return std::distance(&*from, word_end);
  }

  U separators_;
};

template <typename U> class quoted_word_iterator : public word_iterator<U> {
public:
  using word_iterator<U>::word_iterator;

protected:
  using facet_type = typename word_iterator<U>::facet_type;
  using char_type = typename word_iterator<U>::char_type;
  typename std::iterator_traits<std::add_pointer_t<char_type>>::difference_type
  find_current_word_length() final {
    if (const auto cur_word_begin = *this->word_begin_;
        cur_word_begin == char_type{'\''} ||
        cur_word_begin == char_type{'\"'}) {
      auto word_end = std::find(std::next(this->word_begin_, 1),
                                this->string_end_, cur_word_begin);
      if (word_end != this->string_end_)
        ++word_end;
      return std::distance(this->word_begin_, word_end);
    } else {
      auto&& word_end =
          std::use_facet<facet_type>(this->loc_)
              .scan_is(this->mask_, &*this->word_begin_, this->endptr());
      return std::distance(&*this->word_begin_, word_end);
    }
  }
};

template <typename T>
[[nodiscard]] inline auto
cbegin_wordwise(const T& str, const std::locale& loc = std::locale()) {
  return word_iterator<T>{std::cbegin(str), std::cend(str), loc};
}

template <typename T>
[[nodiscard]] inline auto
cend_wordwise(const T& str, const std::locale& loc = std::locale()) {
  return word_iterator<T>{std::cend(str), std::cend(str), loc};
}

template <typename T>
[[nodiscard]] inline auto
cbegin_wordwise_special_seps(const T& str,
                          const std::locale& loc = std::locale()) {
  return word_iterator_spec_seps<T>{std::cbegin(str), std::cend(str), loc};
}

template <typename T>
[[nodiscard]] inline auto
cend_wordwise_special_seps(const T& str, const std::locale& loc = std::locale()) {
  return word_iterator_spec_seps<T>{std::cend(str), std::cend(str), loc};
}

template <typename T>
[[nodiscard]] inline auto
cbegin_wordwise_quoted(const T& str, const std::locale& loc = std::locale()) {
  return quoted_word_iterator<T>{std::cbegin(str), std::cend(str), loc};
}

template <typename T>
[[nodiscard]] inline auto
cend_wordwise_quoted(const T& str, const std::locale& loc = std::locale()) {
  return quoted_word_iterator<T>{std::cend(str), std::cend(str), loc};
}
} // namespace iterator
} // namespace flt::string

#endif
