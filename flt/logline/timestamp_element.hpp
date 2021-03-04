#ifndef FLT_LOGLINES_TIMESTAMP_ELEMENT_HPP
#define FLT_LOGLINES_TIMESTAMP_ELEMENT_HPP

#include <cassert>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <string>

#ifdef __GLIBCXX__
#ifndef FLT_BROKEN_TIME_GET
#define FLT_BROKEN_TIME_GET 1
#endif
#endif

namespace flt::logline::elements {
class log_timestamp {
public:
  static auto from_iso8601() {
    using namespace std::literals::string_literals;
    return log_timestamp{"%Y-%m-%dT%H:%M:%S"s};
  }
  static auto from_traditional() {
    using namespace std::literals::string_literals;
    return log_timestamp{"%b%t%d %H:%M:%S"s};
  }

  log_timestamp(std::string timestamp_format)
      : timestamp_fmt_{std::move(timestamp_format)} {
    const auto tp_now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(tp_now);
#ifdef _MSC_VER
    // MSVC will emit deprecation warnings if localtime or std::localtime are
    // being used. This is because localtime is not thread safe (see below), and
    // should be considered harmful. This 'localtime_s' is NOT ISO 9899 Annex K
    // compatible, and the call will only work in MSVC's runtime. Annex K could
    // solve the same problem but given the fact that it's not part of C++ / ISO
    // 14882:2017 and has seen virtually no implementation, it's pointless to
    // add it here.
    auto mytm = std::tm{};
    auto parts = &mytm;
    const auto lterr = localtime_s(parts, &now_c);
    assert(lterr != EINVAL);
#else
    // Note: std::localtime is generally NOT thread-safe. It can be returning a
    // pointer to TLS static storage (e.g. on MSVC), but it doesn't need to. The
    // reason we obtain the current year in this constructor is so that only the
    // constructor is not thread-safe, but the actual parsing can be done in
    // parallel.
    auto parts = std::localtime(&now_c);
#endif
    cur_year_ = parts->tm_year;
#if FLT_BROKEN_TIME_GET
    using namespace std::literals::string_literals;
    is_trad_format_ = (timestamp_fmt_ == "%b%t%d %H:%M:%S"s);
#endif
  }

  [[nodiscard]] auto
  operator()(std::chrono::system_clock::time_point& output_time) const {
    return log_timestamp_parser{*this, output_time};
  }

protected:
  class log_timestamp_parser {
  public:
    template <typename CharT, typename Traits>
    friend auto& operator>>(std::basic_istream<CharT, Traits>& is,
                            log_timestamp_parser&& logtsp) {
      using sentry_type = typename std::decay_t<decltype(is)>::sentry;
      sentry_type mysent{is};
      if (!mysent)
        return is;

      using namespace std::literals::chrono_literals;

      using tp_type = std::decay_t<decltype(logtsp.output_time_)>;
      using tp_dur = typename tp_type::duration;

      auto outptm = std::tm{};

#if FLT_BROKEN_TIME_GET
      // In libstdc++, std::get_time is completely broken and unusable.
      // %b does not work with full month names, %d requires a leading zero, %t
      // and %n do not work at all. This breaks the traditional date format for
      // us, as days are written out with a leading space instead of zero.
      // ISO8601 is not affected by bugs. For the time, the solution is to
      // implement the traditional format directly for libstdc++.
      bool could_parse = false;
      if (logtsp.parent_.is_trad_format_) {
        const auto& tgetfc = std::use_facet<std::time_get<CharT>>(is.getloc());
        auto errflags = std::ios_base::goodbit;
        tgetfc.get_monthname({is}, {}, is, errflags, &outptm);
        is.setstate(errflags);
        if (is) {
          is >> std::ws >> outptm.tm_mday >> outptm.tm_hour;
          if (is.peek() == ':') {
            is.ignore();
            is >> outptm.tm_min;
            if (is.peek() == ':') {
              is.ignore();
              is >> outptm.tm_sec;
              could_parse = static_cast<bool>(is);
            }
          }
        }
      } else if (is >> std::get_time(
                           &outptm, std::data(logtsp.parent_.timestamp_fmt_))) {
        could_parse = true;
      }
      if (could_parse) {
#else
      if (is >>
          std::get_time(&outptm, std::data(logtsp.parent_.timestamp_fmt_))) {
#endif
        if (outptm.tm_year == 0) {
          outptm.tm_year = logtsp.parent_.cur_year_;
        }
        auto outptime_t = std::mktime(&outptm);
        if (outptime_t == -1) {
          is.setstate(std::ios_base::failbit);
        } else {
          logtsp.output_time_ =
              std::chrono::system_clock::from_time_t(std::move(outptime_t));
          if (is.peek() == '.') {
            is.ignore();
            auto nanosecs = long{};
            is >> std::setw(6) >> nanosecs >> std::setw(0);
            logtsp.output_time_ += std::chrono::duration_cast<tp_dur>(
                std::chrono::nanoseconds(nanosecs));
          }
          if (is.peek() == '+') {
            is.ignore();
            auto hours = int{}, minutes = int{};
            auto delim = char{};
            is >> std::setw(2) >> hours >> delim >> minutes >> std::setw(0);
            if (delim != ':')
              is.setstate(std::ios_base::failbit);
            else
              logtsp.output_time_ += std::chrono::duration_cast<tp_dur>(
                  std::chrono::hours(hours) + std::chrono::minutes(minutes));
          } else if (is.peek() == 'Z') {
            is.ignore();
          }
        }
      }

      return is;
    }

    log_timestamp_parser(const log_timestamp& parent_parser,
                         std::chrono::system_clock::time_point& output_time)
        : parent_(parent_parser), output_time_(output_time) {}

  protected:
    const log_timestamp& parent_;
    std::chrono::system_clock::time_point& output_time_;
  };

  template <typename CharT, typename Traits>
  friend auto& operator>>(std::basic_istream<CharT, Traits>& is,
                          log_timestamp_parser&& sp);

  const std::string timestamp_fmt_;
  int cur_year_;
#if FLT_BROKEN_TIME_GET
  bool is_trad_format_;
#endif
};
} // namespace flt::logline::elements

#endif
