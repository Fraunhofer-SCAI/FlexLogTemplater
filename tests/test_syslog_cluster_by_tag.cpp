#include <flt/clustering/agglomerative.hpp>
#include <flt/clustering/cluster_utils.hpp>
#include <flt/clustering/distance_classifier.hpp>
#include <flt/logline/syslog.hpp>
#include <flt/parameter_filter/common_regex_filters.hpp>
#include <flt/parameter_filter/filter_array.hpp>
#include <flt/parameter_filter/ipv6_address_filter.hpp>
#include <flt/parameter_filter/regex_filter.hpp>
#include <flt/string/levenshtein_distance.hpp>
#include <flt/string/weighted_edit_distance.hpp>
#include <flt/string/word_iterator.hpp>
#include <flt/util/cache_adaptors.hpp>
#include <flt/util/sigmoids.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#if __has_include(<execution> )
#include <execution>
#endif

int main(int argc, char** argv) {
  auto t_start_tot = std::chrono::high_resolution_clock::now();

  using namespace flt::logline;
  using namespace flt::logline::syslog;

  using namespace std::literals;

  std::ios_base::sync_with_stdio(false);
  const auto parsestr = "${IGNORE} ${IGNORE} ${IGNORE} ${MESSAGE}"sv;
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
  // auto f11 = approx_ipv6_address_filter();
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

  auto t_start_filts = std::chrono::high_resolution_clock::now();
  std::for_each(
#if __has_include(<execution>)
      std::execution::par_unseq,
#endif
      std::begin(linesvec), std::end(linesvec),
      [&](auto&& logline) { logline.set_message(w(logline.get_message())); });
  auto t_end_filts = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration_filts = t_end_filts - t_start_filts;

  {
    auto fout = std::ofstream{std::string{argv[1]} + "_filtered"};
    for (auto&& logline : linesvec) {
      fout << "Process '" << logline.get_process();
      if (auto&& mypid = logline.get_pid(); mypid)
        fout << "' with PID '" << mypid.value();
      fout << "' wrote '" << logline.get_message() << "'" << '\n';
    }
    fout.flush();
  }

  auto map_loglines =
      std::unordered_map<std::string_view,
                         std::unordered_set<std::string_view>>{};
  for (auto&& logline : linesvec) {
    auto& set = map_loglines[logline.get_process()];
    set.insert(logline.get_message());
  }

  using flt::string::levenshtein_distance;
  using flt::string::weighted_edit_distance;
  using flt::util::logistic_decrease;
  using flt::util::symmetric_cache_adaptor;
  using flt::util::symmetric_sync_cache_adaptor;
  using namespace flt::string::iterator;

  auto setting = "std";
  auto weight_f = [](const auto k) { return logistic_decrease(k, -1); };
  auto dist_f = [](const auto& a, const auto& b) {
    return levenshtein_distance(a, b);
  };

  using namespace flt::string;
  auto&& cached_dist_f =
      symmetric_sync_cache_adaptor<decltype(dist_f), std::string_view,
                                   std::string_view>{dist_f};
  auto wed_f = [&](const auto& a, const auto& b) {
    return weighted_edit_distance(
        cbegin_wordwise_quoted(a), cend_wordwise_quoted(a),
        cbegin_wordwise_quoted(b), cend_wordwise_quoted(b), cached_dist_f,
        weight_f);
  };
  auto&& cached_wed_f =
      symmetric_sync_cache_adaptor<decltype(wed_f), std::string_view,
                                   std::string_view>{wed_f};

  //   for (auto&& left_s : v) {
  //     std::for_each(
  // #if __has_include(<execution>)
  //         std::execution::par_unseq,
  // #endif
  //         std::cbegin(v), std::cend(v),
  //         [&](auto&& right_s) { cached_wed_f(left_s, right_s); });
  //   }

  auto proc_num = 1;
  auto counter = 0;
  auto pad_zeroes = static_cast<int>(
      std::to_string(std::size(linesvec) * std::size(linesvec)).length());
  auto sets_size = map_loglines.size();
  auto fout = std::ofstream{std::string{argv[1]} + "_clustered"};

  auto duration_wed = std::chrono::duration<double>{};
  auto duration_clust = std::chrono::duration<double>{};

  for (auto&& bucket : map_loglines) {
    if (bucket.second.size() < 2)
      continue;
    std::cout << "Clustering process " << proc_num << " of " << sets_size
              << " (" << std::string{bucket.first} << ")" << std::endl;
    // Sort loglines alphabetically for easier debugging
    auto set = std::set<std::string_view>{};
    for (auto it = bucket.second.begin(); it != bucket.second.end();) {
      set.insert(std::move(bucket.second.extract(it++).value()));
    }

    auto num_wed = decltype(set.size()){0};
    for (auto set_size = set.size() - 1; set_size > 0; --set_size)
      num_wed += set_size;
    std::ostringstream ss;
    ss << std::setw(pad_zeroes) << std::setfill('0') << num_wed;
    auto num_wed_padded = ss.str();
    auto proc_name = std::string{bucket.first};
    std::replace(proc_name.begin(), proc_name.end(), '/', '_');
    auto fout_base = std::string{argv[1]} + "_" + num_wed_padded + "_" +
                     proc_name + "_" + setting;

    auto fout_wed_values = std::ofstream{fout_base + "_wed-values"};
    auto v_wed = std::vector<double>{};
    v_wed.reserve(num_wed);
    auto i = 0;
    auto t_start_wed = std::chrono::high_resolution_clock::now();
    for (auto&& left_s = set.begin(); left_s != set.end()--; ++left_s) {
      for (auto&& right_s = std::next(left_s); right_s != set.end();
           ++right_s) {
        ++i;
        if (i % 1000000 == 0)
          std::cout << "Calc WED " << i << " of " << num_wed << std::endl;
        auto wed = cached_wed_f(*left_s, *right_s);
        fout_wed_values << wed << '\n';
        v_wed.push_back(wed);
      }
    }
    auto t_end_wed = std::chrono::high_resolution_clock::now();
    duration_wed += t_end_wed - t_start_wed;

    using flt::clustering::distance_classification_threshold;
    const auto cutoff = distance_classification_threshold(v_wed);
    fout_wed_values << cutoff << '\n';
    fout_wed_values.flush();

    auto edge_f = [&, cutoff](auto a, auto b) {
      return (cached_wed_f(a, b) < cutoff);
    };
    auto r = std::vector<decltype(set)>{};

    auto t_start_clust = std::chrono::high_resolution_clock::now();
    flt::clustering::agglomerative_clustering(std::back_inserter(r), set,
                                              edge_f);
    auto t_end_clust = std::chrono::high_resolution_clock::now();
    duration_clust += t_end_clust - t_start_clust;

    for (auto&& log_cluster : r) {
      for (auto&& logli : log_cluster) {
        fout << counter << " -> " << logli << '\n';
      }
      ++counter;
    }

    {
      auto fout_inner = std::ofstream{fout_base + "_clustered"};
      auto counter_inner = 0;
      for (auto&& log_cluster : r) {
        for (auto&& logli : log_cluster) {
          fout_inner << counter_inner << " -> " << logli << '\n';
        }
        ++counter_inner;
      }
    }
    ++proc_num;
  } // loop over sets in map

  fout.flush();

  auto t_end_tot = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration_tot = t_end_tot - t_start_tot;

  std::cout << "Duration of filters: " << duration_filts.count() << "\n";
  std::cout << "Duration of WED calc: " << duration_wed.count() << "\n";
  std::cout << "Duration of clustering: " << duration_clust.count() << "\n";
  std::cout << "Duration total: " << duration_tot.count() << "\n";
}
