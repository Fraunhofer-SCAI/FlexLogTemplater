#include <flt/logline/syslog.hpp>

#include <cassert>
#include <fstream>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

int main(int argc, char** argv) {
  using namespace flt::logline::syslog;
  using namespace std::literals;

  const auto parsestr = "${DATE} ${ORIGIN} ${MESSAGE}"sv;
  auto logparser = logline_parser{parsestr};
  auto linesvec = std::vector<logline>{};
  if (argc < 2) {
    std::cerr << "No filename passed" << std::endl;
    return -1;
  }
  auto sf = std::ifstream{argv[1]};
  if (!sf) {
    std::cerr << "Couldn't open file" << std::endl;
    return -1;
  }
  sf.exceptions(std::ios_base::failbit);
  while (!(sf >> std::ws).eof()) {
    logline testline;
    assert(sf);
    sf >> logparser(testline);
    linesvec.push_back(std::move(testline));
  }
  std::cout << "Parsed " << std::size(linesvec) << " loglines" << std::endl;
  for (auto&& logline : linesvec) {
    std::cout << "Process '" << logline.get_process();
    if (auto&& mypid = logline.get_pid(); mypid)
      std::cout << "' with PID '" << mypid.value();
    std::cout << "' wrote '" << logline.get_message() << "'" << std::endl;
    std::cin.get();
  }
}
