#include <flt/logline/host_element.hpp>
#include <flt/logline/timestamp_element.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

int main() {
  using namespace flt::logline::elements;

  using namespace std::literals;

  const auto test_line1 =
      "2018-05-02T12:41:35+02:00 blub.local 2018-05-03T15:18:11Z 127.0.0.1 2001:db8::2:1"s;
  auto test_line_is = std::istringstream{test_line1};

  auto a = log_host{}, b = log_host{}, e = log_host{};
  auto c = std::chrono::system_clock::time_point{},
       d = std::chrono::system_clock::time_point{};
  auto iso_ps = log_timestamp::from_iso8601();
  test_line_is >> iso_ps(c) >> a >> iso_ps(d) >> b >> e;
  auto acdiff = std::chrono::duration<double>(d - c);
  auto td = std::chrono::system_clock::to_time_t(d);
  auto tc = std::chrono::system_clock::to_time_t(c);
#ifdef _MSC_VER
  auto tdtm = std::tm{}, tctm = std::tm{};
  gmtime_s(&tdtm, &td);
  gmtime_s(&tctm, &tc);
#else
  auto gmp = std::gmtime(&td);
  auto tdtm = *gmp;
  gmp = std::gmtime(&tc);
  auto tctm = *gmp;
#endif
  std::cout << acdiff.count() << '\n'
            << std::put_time(&tdtm, "%Y-%m-%d %H:%M:%S") << '\n'
            << std::put_time(&tctm, "%Y-%m-%d %H:%M:%S") << '\n'
            << a << '\n'
            << b << '\n'
            << e << std::endl;
}
