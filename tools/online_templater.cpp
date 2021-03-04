#include <flt/parameter_filter/common_regex_filters.hpp>
#include <flt/parameter_filter/filter_array.hpp>
#include <flt/parameter_filter/ipv6_address_filter.hpp>
#include <flt/templating/online_templater.hpp>

#include <fstream>
#include <iostream>

int main(int argc, char** argv) {

  if (argc != 3) {
    std::cerr << "Invalid number of arguments. Need input log file, output "
                 "templates file"
              << std::endl;
    return -1;
  }

  auto is_log = std::ifstream{argv[1]};
  if (!is_log) {
    std::cerr << "Couldn't open log file " << argv[1] << std::endl;
    return -1;
  }


  using namespace flt::parameter_filter::regex_filters;
  using namespace flt::parameter_filter;

  auto f1 = pointed_bracket_filter();
  auto f2 = square_bracket_filter();
  auto f3 = hexadec_constant_filter();
  auto f4 = ipv4_address_filter();
  auto f5 = mac_address_filter();
  auto f6 = linux_netif_filter();
  auto f7 = time_duration_filter();
  auto f8 = data_size_filter();
  auto f9 = linux_mem_size_filter();
  auto f10 = linux_kernel_audit_filter();
  auto f11 = ipv6_address_filter<>();
  auto f12 = libvirtd_filter();
  auto f13 = UUID_filter();
  auto f14 = time_filter();
  auto f15 = date_filter();
  auto f16 = long_date_filter();
  auto f17 = extended_date_filter();
  auto f99 = number_constant_filter();
  auto w = filter_array<std::vector<std::string>::iterator, std::string>{
      f99, f1,  f2,  f3,  f4,  f5,  f6,  f7,  f8,
      f9,  f10, f11, f12, f13, f14, f15, f16, f17};

  std::cout << "Filtering log file: " << argv[1] << std::endl;
  std::cout << "Templating log file: " << argv[1] << std::endl;
  auto loglines = std::set<std::string>{};
  auto logline = std::string{};
  while (std::getline(is_log, logline))
    loglines.insert(w(logline));

  
  std::cout << "Templating log file: " << argv[1] << std::endl;
  using namespace flt::templating::online;
  auto templater = online_templater<std::string>(0.34, true);
  for (const auto& logline : loglines) {
    templater(logline);
  }

  templater.split_templs();

  auto fout = std::ofstream{argv[2]};
  templater.save_templs(fout);
  fout.close();

  auto fout_pars = std::ofstream{std::string{argv[2]} + "_pars"};
  templater.save_templs(fout_pars, true);
  fout_pars.close();

  return 0;
}
