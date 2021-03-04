#ifndef FLT_PARAMETER_FILTER_COMMON_REGEX_FILTERS_HPP
#define FLT_PARAMETER_FILTER_COMMON_REGEX_FILTERS_HPP

#include <flt/parameter_filter/regex_filter.hpp>

#include <regex>
#include <string>
#include <utility>

namespace flt::parameter_filter::regex_filters {
using namespace std::literals::string_literals;

inline auto pointed_bracket_filter() {
  return regex_filter(std::regex{R"(<.*?>)"s, std::regex::optimize}, "<$$v>"s);
}

inline auto square_bracket_filter() {
  return regex_filter(std::regex{R"(\[.*?\])"s, std::regex::optimize},
                      "[$$v]"s);
}

inline auto hexadec_constant_filter() {
  return regex_filter(
      std::regex{R"(\b0x[[:xdigit:]]+\b)"s, std::regex::optimize});
}

inline auto mac_address_filter() {
  return regex_filter(
      std::regex{R"(\b(?:[[:xdigit:]]{2}[:-]){5}[[:xdigit:]]{2}\b)"s,
                 std::regex::optimize});
}

inline auto ipv4_address_filter() {
  return regex_filter(std::regex{
      R"(\b(?:(?:25[0-5]|2[0-4]?[0-9]?|1\d{0,2}|[1-9][0-9]?|0)\.){3}(?:25[0-5]|2[0-4]?[0-9]?|1\d{0,2}|[1-9][0-9]?|0)(?::\d{1,5})?\b)"s,
      std::regex::optimize});
}

inline auto time_duration_filter() {
  return regex_filter(std::regex{
      R"(\b-?\d+(?:\.\d+)?\s*(?:ms|s|seconds?)\b)"s, std::regex::optimize});
}

inline auto data_size_filter() {
  return regex_filter(std::regex{R"(\b\d+(?:\.\d+)?\s*[MKGTP]?i?B\b)"s,
                                 std::regex::optimize | std::regex::icase});
}

inline auto UUID_filter() {
  return regex_filter(std::regex{
      R"(\b\{?[[:xdigit:]]{8}-(?:[[:xdigit:]]{4}-){3}[[:xdigit:]]{12}\}?\b)"s,
      std::regex::optimize});
}

inline auto number_constant_filter() {
  return regex_filter(
      std::regex{R"((^|[[:space:](=/\\'"%#@:.])(-?\d+)(?=[[:space:])/\\'",%#@:]|$))"s,
                 std::regex::optimize},
      "$1$$v"s, "$2"s);
}

inline auto aggressive_number_constant_filter() {
  return regex_filter(std::regex{R"(\b-?\d+\b)"s, std::regex::optimize},
                      "$1$$v"s, "$2"s);
}

inline auto time_filter() {
  return regex_filter(
      std::regex{R"(\b\d{2}:\d{2}:\d{2}\b)"s, std::regex::optimize});
}

inline auto date_filter() {
  return regex_filter(
      std::regex{R"(\b\d{2}/\d{2}/\d{2}\b)"s, std::regex::optimize});
}

inline auto long_date_filter() {
  return regex_filter(std::regex{
      R"(\b(?:[[:upper:]][[:lower:]]{2}\s+){0,2}\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\s+[[:upper:]]{3})?\b)"s,
      std::regex::optimize});
}

inline auto extended_date_filter() {
  return regex_filter(std::regex{
      R"(\b(?:[[:upper:]][[:lower:]]{2}\s+){2}\d{2}(?:\s+\d{4})?\s+\d{2}:\d{2}:\d{2}(?:\s+[[:upper:]]{3}(?:\+\d{4})?)?(?:\s+\d{4})?\b)"s,
      std::regex::optimize});
}

inline auto linux_mem_size_filter() {
  return regex_filter(std::regex{R"(\b\d+(?:\.\d+)?\s*[GKM]\b)"s,
                                 std::regex::optimize | std::regex::icase});
}

inline auto linux_netif_filter() {
  // There is a number of possible naming schemes for predictable network
  // interface device names. Details here:
  // https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/7/html/networking_guide/sec-understanding_the_predictable_network_interface_device_names
  return regex_filter(std::regex{
      R"(\b(?:(?:P\d+)?(?:en|wl|ww)(?:o\d+|x[[:xdigit:]]{12}|(?:p\d+)?s\d+(?:f\d+)?(?:d\d+)?|p\d+s\d+(?:f\d+)?(?:u\d+)*(?:c\d+)?(?:i\d+)?)|(?:eth|wlan|wwan)\d+)\b)"s,
      std::regex::optimize});
}

inline auto linux_kernel_audit_filter() {
  // If the Linux kernel is configured for auditing, there might be audit
  // messages in our logs. These have a token 'audit(time_stamp:ID)' with the ID
  // being a unique record and the timestamp being a high precision Unix time.
  // More details can be found here:
  // https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/7/html/security_guide/sec-understanding_audit_log_files
  return regex_filter(
      std::regex{
          R"(\baudit\(\d+\.\d+:\d+\))"s, std::regex::optimize},
      "audit($$v)"s);
}

inline auto libvirtd_filter() {
  return regex_filter(std::regex{
      R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\+\d{4}: \d+:)"s,
      std::regex::optimize});
}

inline auto separation_inserter() {
  return regex_filter(
      std::regex{
          R"(([:,.=/\\"'])([[:alnum:]]))"s, std::regex::optimize},
      "$1 $2"s);
}

} // namespace flt::parameter_filter::regex_filters

#endif
