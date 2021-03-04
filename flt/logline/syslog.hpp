#ifndef FLT_LOGLINES_SYSLOG_HPP
#define FLT_LOGLINES_SYSLOG_HPP

#include <flt/logline/host_element.hpp>
#include <flt/logline/timestamp_element.hpp>

// pid_t is a signed integer type that is not greater than the width of the type
// long. This is typically a 32-bit integer.
// Windows does have a minimalistic <sys/types.h> header in its ucrt, but pid_t
// is not defined within.
#ifdef _MSC_VER
#include <cstdint>
using pid_t = std::int_fast32_t;
#elif __has_include(<sys/types.h>)
#include <sys/types.h>
#else
using pid_t = long;
#endif

#include <algorithm>
#if __has_include(<charconv>)
#include <charconv>
#else
#include <cstdlib>
#endif
#include <chrono>
#include <iosfwd>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace flt::logline {
inline namespace syslog {
struct logline {
  using string_type = std::string;
  using pid_type = pid_t;
  using time_point_type = std::chrono::system_clock::time_point;

  enum class Facility : int {
    kern,
    user,
    mail,
    daemon,
    auth,
    lpr,
    news,
    uucp,
    clock,
    authpriv,
    ftp,
    ntp,
    audit,
    alert,
    cron,
    local0,
    local1,
    local2,
    local3,
    local4,
    local5,
    local6,
    local7
  };

  enum class Severity : int {
    emerg,
    alert,
    crit,
    err,
    warning,
    notice,
    info,
    debug
  };

  [[nodiscard]] operator std::string_view() const {
    return static_cast<std::string_view>(message_);
  }

  void set_full_message(const std::string& full_message) {
    const auto delimpos = full_message.find(':');
    if (delimpos == std::string::npos || delimpos < 1) {
      throw std::invalid_argument{
          "Message passed does not seem to be a syslog message"};
    }
    message_ = full_message.substr(delimpos + 2);
    auto processpos = delimpos;
    if (full_message[delimpos - 1] == ']') {
      const auto bracketpos = full_message.rfind('[', delimpos);
      if (bracketpos == std::string::npos) {
        throw std::invalid_argument{
            "Syslog message with invalid processname given"};
      }
      const auto firstpidchar = std::data(full_message) + bracketpos + 1;
#if __has_include(<charconv>)
      const auto lastpidchar = std::data(full_message) + delimpos - 1;
      auto pid = pid_type{};
      const auto fromcharsres = std::from_chars(firstpidchar, lastpidchar, pid);
      pid_ = std::move(pid);
      if (fromcharsres.ptr != lastpidchar) {
#else
      char* lastpidchar;
      auto temppid = std::strtoll(firstpidchar, &lastpidchar, 10);
      pid_ = static_cast<pid_t>(temppid);
      if (lastpidchar == firstpidchar) {
#endif
        throw std::invalid_argument{"Message with invalid PID was passed"};
      }
      processpos = bracketpos;
    }
    process_ = full_message.substr(0, processpos);
  }

  void set_message(std::string message) { message_ = std::move(message); }
  [[nodiscard]] const auto& get_message() const { return message_; }
  [[nodiscard]] auto get_full_message() const {
    auto mysstr = std::ostringstream{};
    mysstr << process_;
    if (pid_.has_value()) {
      mysstr << '[' << pid_.value() << ']';
    }
    mysstr << ": " << message_;
    return mysstr.str();
  }
  [[nodiscard]] auto get_tagged_message() const {
    auto mysstr = std::ostringstream{};
    mysstr << process_ << ": " << message_;
    return mysstr.str();
  }
  [[nodiscard]] const auto& get_process() const { return process_; }
  [[nodiscard]] const auto& get_pid() const { return pid_; }

  [[nodiscard]] const auto& get_origin() const { return origin_; }
  void set_origin(elements::log_host lh) { origin_ = std::move(lh); }
  [[nodiscard]] const auto& get_severity() const { return severity_; }
  void set_severity(Severity sv) { severity_ = std::move(sv); }
  [[nodiscard]] const auto& get_facility() const { return facility_; }
  void set_facility(Facility fc) { facility_ = std::move(fc); }
  [[nodiscard]] const auto& get_timestamp() const { return timestamp_; }
  void set_timestamp(time_point_type ts) { timestamp_ = std::move(ts); }

protected:
  std::optional<time_point_type> timestamp_;
  std::optional<Facility> facility_;
  std::optional<Severity> severity_;
  std::optional<elements::log_host> origin_;
  std::string message_;
  std::string process_;
  std::optional<pid_t> pid_;
}; // namespace syslog

class logline_parser {
public:
  logline_parser(const std::string_view& logline_format,
                 const std::locale& loc = std::locale{}) {
    using namespace std::literals::string_view_literals;
    using namespace std::literals::string_literals;
    const auto syslog_token_map =
        std::unordered_map<std::string_view, syslog_token_types>{
            {"ORIGIN"sv, syslog_token_types::Origin},
            {"FACILITY"sv, syslog_token_types::Facility},
            {"FACILITY_NUM"sv, syslog_token_types::FacilityNum},
            {"ISODATE"sv, syslog_token_types::ISODate},
            {"DATE"sv, syslog_token_types::TradDate},
            {"SEVERITY"sv, syslog_token_types::Severity},
            {"SEVERITY_NUM"sv, syslog_token_types::SeverityNum},
            {"MESSAGE"sv, syslog_token_types::Message},
            {"IGNORE"sv, syslog_token_types::Ignore}};
    for (auto it = std::cbegin(logline_format),
              logf_end_it = std::cend(logline_format);
         it != logf_end_it; ++it) {
      if (*it == '$') {
        const auto next_it = it + 1;
        if (next_it != logf_end_it && *next_it == '{') {
          const auto keyword_it = next_it + 1;
          const auto end_it = std::find(keyword_it, logf_end_it, '}');
          if (end_it == logf_end_it)
            throw std::invalid_argument{"Invalid syslog format string"};
          const auto keyword_sv = std::string_view{
              &*keyword_it, static_cast<std::string_view::size_type>(
                                std::distance(keyword_it, end_it))};
          auto token_it = syslog_token_map.find(keyword_sv);
          if (token_it == std::cend(syslog_token_map)) {
            throw std::invalid_argument{"Unknown syslog macro ${"s +
                                        std::data(keyword_sv) + '}'};
          } else if (token_it->second == syslog_token_types::Message &&
                     (end_it + 1) != logf_end_it) {
            throw std::invalid_argument{
                "${MESSAGE} is only supported as final argument"};
          }
          tokens_.emplace_back(syslog_token{token_it->second, 0});
          it = end_it;
          continue;
        }
      }
      if (!std::isspace(*it, loc))
        tokens_.emplace_back(syslog_token{syslog_token_types::CharToken, *it});
    }
  }

  [[nodiscard]] auto operator()(logline& output_log) const {
    return logline_stream_parser{*this, output_log};
  }

protected:
  const elements::log_timestamp iso_stamp_reader_ =
      elements::log_timestamp::from_iso8601();
  const elements::log_timestamp trad_stamp_reader_ =
      elements::log_timestamp::from_traditional();

#define FAC_MAPTOKEN(X)                                                        \
  { "X", logline::Facility::X }

  const std::unordered_map<std::string_view, logline::Facility> fact_map_{
      FAC_MAPTOKEN(kern),     FAC_MAPTOKEN(user),   FAC_MAPTOKEN(mail),
      FAC_MAPTOKEN(daemon),   FAC_MAPTOKEN(auth),   FAC_MAPTOKEN(lpr),
      FAC_MAPTOKEN(news),     FAC_MAPTOKEN(uucp),   FAC_MAPTOKEN(clock),
      FAC_MAPTOKEN(authpriv), FAC_MAPTOKEN(ftp),    FAC_MAPTOKEN(ntp),
      FAC_MAPTOKEN(audit),    FAC_MAPTOKEN(alert),  FAC_MAPTOKEN(cron),
      FAC_MAPTOKEN(local0),   FAC_MAPTOKEN(local1), FAC_MAPTOKEN(local2),
      FAC_MAPTOKEN(local3),   FAC_MAPTOKEN(local4), FAC_MAPTOKEN(local5),
      FAC_MAPTOKEN(local6),   FAC_MAPTOKEN(local7)};

#undef FAC_MAPTOKEN

#define SEVER_MAPTOKEN(X)                                                      \
  { "X", logline::Severity::X }

  const std::unordered_map<std::string_view, logline::Severity> sever_map_{
      SEVER_MAPTOKEN(emerg), SEVER_MAPTOKEN(alert),   SEVER_MAPTOKEN(crit),
      SEVER_MAPTOKEN(err),   SEVER_MAPTOKEN(warning), SEVER_MAPTOKEN(notice),
      SEVER_MAPTOKEN(info),  SEVER_MAPTOKEN(debug)};

#undef SEVER_MAPTOKEN

  class logline_stream_parser {
  public:
    logline_stream_parser(const logline_parser& parent_parser, logline& logline)
        : parent_parser_{parent_parser}, logline_{logline} {}

    template <typename CharT, typename Traits>
    friend auto& operator>>(std::basic_istream<CharT, Traits>& is,
                            logline_stream_parser&& sp) {
      using sentry_type = typename std::decay_t<decltype(is)>::sentry;
      sentry_type mysent{is};
      if (!mysent)
        return is;
      using namespace std::literals::string_literals;
      const auto& parser = sp.parent_parser_;
      for (auto&& logtok : parser.tokens_) {
        switch (logtok.type) {
        case logline_parser::syslog_token_types::CharToken: {
          char next_ch;
          is >> next_ch;
          if (is.fail() || next_ch != logtok.char_value) {
            throw std::logic_error{"Unexpected character found in stream"};
          }
          break;
        }
        case logline_parser::syslog_token_types::Origin: {
          std::string next_string_tok;
          is >> next_string_tok;
          if (is)
            sp.logline_.set_origin(
                elements::log_host{std::move(next_string_tok)});
          break;
        }
        case logline_parser::syslog_token_types::Facility: {
          std::string next_string_tok;
          is >> next_string_tok;
          if (const auto it = sp.parent_parser_.fact_map_.find(next_string_tok);
              is && it != std::cend(sp.parent_parser_.fact_map_)) {
            sp.logline_.set_facility(it->second);
          } else {
            throw std::logic_error{"Unexpected facility "s + next_string_tok +
                                   " found"};
          }
          break;
        }
        case logline_parser::syslog_token_types::Severity: {
          std::string next_string_tok;
          is >> next_string_tok;
          if (const auto it =
                  sp.parent_parser_.sever_map_.find(next_string_tok);
              is && it != std::cend(sp.parent_parser_.sever_map_))
            sp.logline_.set_severity(it->second);
          break;
        }
        case logline_parser::syslog_token_types::FacilityNum: {
          auto facnum = std::underlying_type_t<logline::Facility>{};
          is >> facnum;
          if (is)
            sp.logline_.set_facility(logline::Facility{facnum});
          break;
        }
        case logline_parser::syslog_token_types::SeverityNum: {
          auto severnum = std::underlying_type_t<logline::Severity>{};
          is >> severnum;
          if (is)
            sp.logline_.set_severity(logline::Severity{severnum});
          break;
        }
        case logline_parser::syslog_token_types::Message: {
          auto strbuf = std::string{};
          std::getline(is >> std::ws, strbuf);
          sp.logline_.set_full_message(std::move(strbuf));
          break;
        }
        case logline_parser::syslog_token_types::ISODate: {
          auto ts = logline::time_point_type{};
          is >> sp.parent_parser_.iso_stamp_reader_(ts);
          sp.logline_.set_timestamp(std::move(ts));
          break;
        }
        case logline_parser::syslog_token_types::TradDate: {
          auto ts = logline::time_point_type{};
          is >> sp.parent_parser_.trad_stamp_reader_(ts);
          sp.logline_.set_timestamp(std::move(ts));
          break;
        }
        case logline_parser::syslog_token_types::Ignore: {
          for (const auto loc = is.getloc();
               (Traits::eof() != is.peek() && std::isspace(is.peek(), loc));
               is.ignore())
            ;
          break;
        }
        }
      }
      return is;
    }

  protected:
    const logline_parser& parent_parser_;
    logline& logline_;
  };

  template <typename CharT, typename Traits>
  friend auto& operator>>(std::basic_istream<CharT, Traits>& is,
                          logline_stream_parser&& sp);

  enum class syslog_token_types {
    CharToken,
    Origin,
    Facility,
    FacilityNum,
    ISODate,
    TradDate,
    Severity,
    SeverityNum,
    Message,
    Ignore
  };

  struct syslog_token {
    syslog_token_types type;
    char char_value;
  };

  std::vector<syslog_token> tokens_;
};
} // namespace syslog
} // namespace flt::logline

#endif
