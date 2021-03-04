#ifndef PARAMETER_FILTER_REGEX_FILTER_HPP
#define PARAMETER_FILTER_REGEX_FILTER_HPP

#include <algorithm>
#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <utility>

namespace flt::parameter_filter {
template <typename CharT, typename RegexTraits,
          typename StringType = std::basic_string<CharT>>
class regex_filter {
protected:
  using regex_type = std::basic_regex<CharT, RegexTraits>;

public:
  regex_filter(regex_type regex, StringType replacement_str = StringType{"$$v"},
               StringType extraction_str = StringType{"$&"},
               std::regex_constants::match_flag_type regex_flags =
                   std::regex_constants::format_default)
      : regex_(std::move(regex)), replacement_str_(std::move(replacement_str)),
        extraction_str_(std::move(extraction_str)),
        regex_flags_(std::move(regex_flags)) {}

  template <typename OutIt, typename T>
  auto operator()(const T& line_str, OutIt output_cont) const {
    return apply_filter(line_str, output_cont);
  }

  template <typename T> auto operator()(const T& line_str) const {
    return apply_filter(line_str, nullptr);
  }

protected:
  template <typename T, typename Its>
  auto apply_filter(const T& line_str, [[maybe_unused]] Its output_it) const {
    auto res_str = T{};
    auto res_str_it = std::back_inserter(res_str);
    using regex_it = std::regex_iterator<typename T::const_iterator>;
    auto regex_i_last = regex_it{};
    for (auto regex_i =
                  regex_it{std::cbegin(line_str), std::cend(line_str), regex_},
              regex_end = regex_it{};
         regex_i != regex_end; regex_i_last = regex_i++) {
      const auto regex_m = *regex_i;
      res_str_it = std::copy(std::get<0>(regex_m.prefix()),
                             std::get<1>(regex_m.prefix()), res_str_it);

      if constexpr (!std::is_same_v<Its, std::nullptr_t>) {
        auto extr_str = T{};
        auto extr_str_it = std::back_inserter(extr_str);
        extr_str_it = regex_m.format(extr_str_it, extraction_str_);
        *output_it = std::move(extr_str);
        ++output_it;
      }

      res_str_it = regex_m.format(res_str_it, replacement_str_);
    }
    if (regex_i_last != regex_it{}) {
      const auto regex_m_last = *regex_i_last;
      res_str_it = std::copy(std::get<0>(regex_m_last.suffix()),
                             std::get<1>(regex_m_last.suffix()), res_str_it);
    } else {
      res_str = line_str;
    }
    return std::move(res_str);
  }

  const regex_type regex_;
  const StringType replacement_str_;
  const StringType extraction_str_;
  const std::regex_constants::match_flag_type regex_flags_;
};

} // namespace flt::parameter_filter

#endif
