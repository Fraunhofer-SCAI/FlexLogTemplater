#ifndef PARAMETER_FILTER_IPV6_ADDRESS_FILTER_HPP
#define PARAMETER_FILTER_IPV6_ADDRESS_FILTER_HPP

#include <algorithm>
#include <functional>
#include <regex>
#include <string>
#include <utility>

namespace flt::parameter_filter {
template <typename StringType = std::string> class ipv6_address_filter {
public:
  ipv6_address_filter(StringType replacement_str = std::string{"$$v"})
      : replacement_str_(std::move(replacement_str)) {}

  template <typename OutIt, typename T>
  auto operator()(const T& line_str, OutIt output_cont) const {
    return apply_filter(line_str, output_cont);
  }

  template <typename T> auto operator()(const T& line_str) const {
    return apply_filter(line_str, nullptr);
  }

protected:
  const std::regex regex_ipv6_begin{R"(\b[[:xdigit:]]{1,4}:|::)",
                                    std::regex::optimize};
  const std::regex regex_ipv6_mid{R"([[:xdigit:]]{1,4}:)",
                                  std::regex::optimize};
  const std::regex regex_ipv6_end{R"([[:xdigit:]]{1,4}(?![\w:]))",
                                  std::regex::optimize};
  const std::regex regex_boundary{R"([^\w])", std::regex::optimize};
  const std::regex regex_col_boundary{R"(:(?![\w:]))", std::regex::optimize};
  const std::regex regex_port{R"(]:\d{1,5}(?![\w:]))", std::regex::optimize};

  template <typename T, typename Its>
  auto apply_filter(const T& line_str, [[maybe_unused]] Its output_it) const {
    auto res_str = T{};
    auto res_str_it = std::back_inserter(res_str);
    using regex_it = std::regex_iterator<typename T::const_iterator>;
    auto regex_m_last = std::smatch{};
    auto first_match = true;
    auto prefix_start = std::cbegin(line_str);
    for (auto regex_i = regex_it{std::cbegin(line_str), std::cend(line_str),
                                 regex_ipv6_begin},
              regex_end = regex_it{};
         regex_i != regex_end; regex_i++) {
      const auto regex_m = *regex_i;
      auto num_colons = 0u;
      auto has_dblcol = false;
      if (regex_m[0] == "::") {
        has_dblcol = true;
        if (!std::regex_match(std::get<1>(regex_m.prefix()) - 1,
                              std::get<1>(regex_m.prefix()), regex_boundary)) {
          continue;
        }
      } else
        num_colons = 1;

      auto bracket_begin = false;
      if (std::get<1>(regex_m.prefix()) != std::cbegin(line_str)) {
        // Do not take ':' as word boundary. C++ regex does not support
        // lookbehind, this is a workaround.
        if (*(std::get<1>(regex_m.prefix()) - 1) == ':')
          continue;
        if (*(std::get<1>(regex_m.prefix()) - 1) == '[')
          bracket_begin = true;
      }

      auto regex_m_cont = std::smatch{};
      auto regex_m_suffix = std::get<0>(regex_m.suffix());
      auto max_colons = 7u;
      auto max_colons_dblcol = 5u;
      auto num_colons_correct = false;
      auto boundary_reached = false;
      auto done_once = false;

      do {
        if (done_once) {
          regex_m_suffix = std::get<0>(regex_m_cont.suffix());
          num_colons++;
          regex_i++;
        }
        done_once = true;
        if (std::regex_search(regex_m_suffix, std::cend(line_str), regex_m_cont,
                              regex_col_boundary,
                              std::regex_constants::match_continuous) &&
            num_colons <= max_colons_dblcol) {
          if (has_dblcol)
            break;
          has_dblcol = true;
          boundary_reached = true;
          regex_m_suffix++;
          break;
        }
        if (*regex_m_suffix == ':') {
          if (has_dblcol)
            break;
          has_dblcol = true;
          regex_m_suffix++;
        }
      } while (!boundary_reached &&
               std::regex_search(regex_m_suffix, std::cend(line_str),
                                 regex_m_cont, regex_ipv6_mid,
                                 std::regex_constants::match_continuous) &&
               num_colons <= max_colons);

      if (!boundary_reached &&
          std::regex_search(regex_m_suffix, std::cend(line_str), regex_m_cont,
                            regex_ipv6_end,
                            std::regex_constants::match_continuous)) {
        boundary_reached = true;
        regex_m_suffix = std::get<0>(regex_m_cont.suffix());
      }

      if (has_dblcol && num_colons <= max_colons_dblcol)
        num_colons_correct = true;
      else if (!has_dblcol && num_colons == max_colons)
        num_colons_correct = true;

      if (num_colons_correct && boundary_reached) {
        auto regex_m_port = std::smatch{};
        auto has_port = false;
        if (bracket_begin &&
            std::regex_search(regex_m_suffix, std::cend(line_str), regex_m_port,
                              regex_port,
                              std::regex_constants::match_continuous)) {
          regex_m_cont = regex_m_port;
          has_port = true;
        }
        res_str_it =
            std::copy(prefix_start + !first_match,
                      std::get<1>(regex_m.prefix()) - has_port, res_str_it);
        res_str_it = regex_m.format(res_str_it, replacement_str_);
        regex_m_last = regex_m_cont;
        prefix_start = std::get<0>(regex_m_last.suffix()) - 1;
        first_match = false;

        if constexpr (!std::is_same_v<Its, std::nullptr_t>) {
          auto extr_str_begin = std::get<0>(regex_m[0]);
          auto extr_str_end = std::get<0>(regex_m_last.suffix());
          auto extr_str = T{};
          auto extr_str_it = std::back_inserter(extr_str);
          extr_str_it = std::copy(extr_str_begin, extr_str_end, extr_str_it);
          *output_it = std::move(extr_str);
          ++output_it;
        }
      }
    }

    if (regex_m_last != std::smatch{}) {
      res_str_it = std::copy(std::get<0>(regex_m_last.suffix()),
                             std::get<1>(regex_m_last.suffix()), res_str_it);
    } else {
      res_str = line_str;
    }
    return std::move(res_str);
  }

  const StringType replacement_str_;
};

} // namespace flt::parameter_filter

#endif
