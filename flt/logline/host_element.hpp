#ifndef FLT_LOGLINES_HOST_ELEMENT_HPP
#define FLT_LOGLINES_HOST_ELEMENT_HPP

#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#elif _WIN32_WINNT < 0x0600
#error _WIN32_WINNT is too old, inet_pton is not be available
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef STRICT
#define STRICT
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <WS2tcpip.h>

#include <mstcpip.h>
#else
extern "C" {
#include <arpa/inet.h>
}
#endif

namespace flt::logline::elements {
class log_host {
public:
  log_host() = default;

  log_host(const std::string& address) {
#ifdef _WIN32
    auto scopeid = ULONG{};
    auto portnum = USHORT{};
#endif
    if (auto in6ad = in6_addr{};
#ifdef _WIN32
        !RtlIpv6StringToAddressExA(std::data(address), &in6ad, &scopeid,
                                   &portnum)
#else
        inet_pton(AF_INET6, std::data(address), &in6ad) == 1
#endif
    ) {
      address_ = std::move(in6ad);
    } else if (auto in4ad = in_addr{};
#ifdef _WIN32
               !RtlIpv4StringToAddressExA(std::data(address), false, &in4ad,
                                          &portnum)
#else
               inet_pton(AF_INET, std::data(address), &in4ad) == 1
#endif
    ) {
      address_ = std::move(in4ad);
    } else {
      address_ = std::forward<decltype(address)>(address);
    }
  }

  [[nodiscard]] const auto& operator()() const { return address_; }

  template <typename CharT, typename Traits>
  friend auto& operator>>(std::basic_istream<CharT, Traits>& is, log_host& lh) {
    using sentry_type = typename std::decay_t<decltype(is)>::sentry;
    sentry_type mysent{is};
    if (!mysent)
      return is;
    auto strbuf = std::string{};
    is >> strbuf;
    lh = log_host{std::move(strbuf)};
    return is;
  }

  template <typename CharT, typename Traits>
  friend auto& operator<<(std::basic_ostream<CharT, Traits>& os,
                          const log_host& lh) {
    using sentry_type = typename std::decay_t<decltype(os)>::sentry;
    sentry_type mysent{os};
    if (!mysent)
      return os;
    std::visit(
        [&os](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, std::string>) {
            os << std::forward<decltype(arg)>(arg);
          } else if constexpr (std::is_same_v<T, in_addr>) {
            char outad[INET_ADDRSTRLEN]{};
#ifdef _WIN32
            auto addr_size = ULONG{sizeof(outad)};
            if (RtlIpv4AddressToStringExA(&arg, 0, outad, &addr_size)) {
#else
            if (inet_ntop(AF_INET, &arg, outad, INET_ADDRSTRLEN) != nullptr) {
#endif
              os.setstate(std::ios_base::failbit);
            } else {
              os << outad;
            }
          } else if constexpr (std::is_same_v<T, in6_addr>) {
            char outad[INET6_ADDRSTRLEN]{};
#ifdef _WIN32
            auto addr_size = ULONG{sizeof(outad)};
            if (RtlIpv6AddressToStringExA(&arg, 0, 0, outad, &addr_size)) {
#else
            if (inet_ntop(AF_INET6, &arg, outad, INET6_ADDRSTRLEN) != nullptr) {
#endif
              os.setstate(std::ios_base::failbit);
            } else {
              os << outad;
            }
          }
        },
        lh.address_);
    return os;
  }

protected:
  std::variant<in_addr, in6_addr, std::string> address_;
};
} // namespace flt::logline::elements

#endif
