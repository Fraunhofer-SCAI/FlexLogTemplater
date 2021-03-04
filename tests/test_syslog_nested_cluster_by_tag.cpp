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
#include <numeric>
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
  const auto parsestr = "${IGNORE} ${IGNORE} ${MESSAGE}"sv;
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
  auto f99 = aggressive_number_constant_filter();
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

  auto logs_by_task =
      std::unordered_map<std::string_view,
                         std::unordered_set<std::string_view>>{};
  for (auto&& logline : linesvec) {
    auto& set = logs_by_task[logline.get_process()];
    set.insert(logline.get_message());
  }

  auto setting = "std";
  auto proc_num = 1;
  auto pad_zeroes = static_cast<int>(
      std::to_string(std::size(linesvec) * std::size(linesvec)).length());
  auto sets_size = logs_by_task.size();

  auto duration_wed = std::chrono::duration<double>{};
  auto duration_clust = std::chrono::duration<double>{};
  auto num_clusters = std::size_t{0};

  for (auto&& bucket : logs_by_task) {
    if (bucket.second.size() < 2)
      continue;
    std::cout << "Clustering process " << proc_num << " of " << sets_size
              << " (" << std::string{bucket.first} << ")" << std::endl;
    // Sort loglines alphabetically for easier debugging
    auto set = std::set<std::string_view>{};
    for (auto it = bucket.second.begin(); it != bucket.second.end();) {
      set.insert(std::move(bucket.second.extract(it++).value()));
    }

    // Cluster by first word of logline
    using namespace flt::string::iterator;
    auto clust_preproc =
        std::unordered_map<std::string, std::set<std::string_view>>{};
    for (auto&& logline : set) {
      auto first_word = std::string(*cbegin_wordwise_quoted(logline));
      auto length = std::distance(cbegin_wordwise_quoted(logline),
                                  cend_wordwise_quoted(logline));
      auto hash = std::to_string(length) + '=' + first_word;
      auto& clust = clust_preproc[hash];
      clust.insert(logline);
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

    auto clust_loc_i = 0u;
    auto v_clustered =
        std::vector<std::pair<decltype(set), std::vector<unsigned int>>>{};

    for (auto&& clust : clust_preproc) {
      auto scaling_factor = 0.;
      {
        auto fout = std::ofstream{fout_base + "_line_lengths_" +
                                  std::to_string(clust_loc_i)};
        auto logli_lens = std::vector<double>{};
        logli_lens.reserve(set.size());
        for (auto&& logli : set) {
          auto logli_len = std::distance(cbegin_wordwise_quoted(logli),
                                         cend_wordwise_quoted(logli));
          logli_lens.push_back(static_cast<double>(logli_len));
          fout << logli_len << '\n';
        }
        auto mean = std::accumulate(logli_lens.begin(), logli_lens.end(), 0.0) /
                    logli_lens.size();
        auto diff = std::vector<double>(logli_lens.size());
        std::transform(logli_lens.begin(), logli_lens.end(), diff.begin(),
                       [mean](double x) { return x - mean; });
        auto sq_sum =
            std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
        auto stdev = std::sqrt(sq_sum / logli_lens.size());
        fout.flush();
        scaling_factor = 7. / (mean + stdev);
      }
      std::cout << "Scaling factor: " << scaling_factor << std::endl;

      using flt::string::levenshtein_distance;
      using flt::string::weighted_edit_distance;
      using flt::util::logistic_decrease;
      using flt::util::symmetric_cache_adaptor;
      using flt::util::symmetric_sync_cache_adaptor;

      auto weight_f = [scaling_factor](const auto k) {
        return logistic_decrease(k, -1., scaling_factor);
      };
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

      // Nested clustering
      auto clust_location = std::vector<unsigned int>{};
      auto cutoffs = std::vector<double>{};

      auto cluster_rec = [&](auto set_logli, auto rec_func) {
        auto clust_loc_str = "clust"s;
        for (auto&& pos : clust_location)
          clust_loc_str += "-" + std::to_string(pos);
        std::cout << "--- Cluster " << clust_loc_str << " with size "
                  << set_logli.size() << " ---" << std::endl;
        if (set_logli.size() == 1) {
          std::cout << "Cluster has size 1" << std::endl;
          v_clustered.push_back(std::make_pair(set_logli, clust_location));
          return;
        }
        auto num_wed_inner = std::size_t{};
        for (auto set_size = set_logli.size() - 1; set_size > 0; --set_size)
          num_wed_inner += set_size;
        auto fout_wed_values =
            std::ofstream{fout_base + "_wed-values_" + clust_loc_str};
        auto v_wed = std::vector<double>{};
        v_wed.reserve(num_wed_inner);
        auto i = 0;

        auto t_start_wed = std::chrono::high_resolution_clock::now();
        for (auto&& left_s = set_logli.begin(); left_s != --set_logli.end();
             ++left_s) {
          for (auto&& right_s = std::next(left_s); right_s != set_logli.end();
               ++right_s) {
            ++i;
            if (i % 1000000 == 0)
              std::cout << "Calc WED " << i << " of " << num_wed_inner
                        << std::endl;
            auto wed = cached_wed_f(*left_s, *right_s);
            fout_wed_values << wed << '\n';
            v_wed.push_back(wed);
          }
        }
        auto t_end_wed = std::chrono::high_resolution_clock::now();
        duration_wed += t_end_wed - t_start_wed;

        auto wed_max =
            *(std::max_element(std::cbegin(v_wed), std::cend(v_wed)));
        auto wed_min =
            *(std::min_element(std::cbegin(v_wed), std::cend(v_wed)));
        std::cout << "Max WED: " << wed_max << std::endl;
        if (std::abs(wed_max) < 0.000001) {
          v_clustered.push_back(std::make_pair(set_logli, clust_location));
          return;
        }
        if (std::abs(wed_max / wed_min - 1) < 0.000001) {
          std::cout << "All WEDs in cluster are equal" << std::endl;
          v_clustered.push_back(std::make_pair(set_logli, clust_location));
          return;
        }

        using flt::clustering::distance_classification_threshold;
        auto cutoff = distance_classification_threshold(v_wed);
        fout_wed_values << cutoff << '\n';
        fout_wed_values.flush();
        std::cout << "Curr cutoff: " << cutoff << std::endl;

        auto clust_depth = clust_location.size();
        if (clust_depth > 1) {
          assert(cutoffs.size() == clust_depth - 1);
          auto cutoff_last = cutoffs.back();
          std::cout << "Last cutoff: " << cutoff_last << std::endl;
          if (cutoff >= 0.9999 * cutoff_last && wed_max >= 1.2 * cutoff_last)
            cutoff = 0.95 * cutoff_last;

          if (cutoff >= 0.999 * cutoff_last || wed_max < cutoff_last ||
              (cutoff < 0.1 * cutoff_last &&
               std::abs(wed_max / cutoff - 1) < 0.9) ||
              (cutoff < 0.5 * cutoff_last &&
               std::abs(wed_max / cutoff - 1) < 0.1)) {
            v_clustered.push_back(std::make_pair(set_logli, clust_location));
            return;
          }
        }
        cutoffs.push_back(cutoff);

        auto edge_f = [&, cutoff](auto a, auto b) {
          return (cached_wed_f(a, b) < cutoff);
        };

        auto v_clust_temp = std::vector<decltype(set)>{};
        auto t_start_clust = std::chrono::high_resolution_clock::now();
        flt::clustering::agglomerative_clustering(
            std::back_inserter(v_clust_temp), set_logli, edge_f);
        auto t_end_clust = std::chrono::high_resolution_clock::now();
        duration_clust += t_end_clust - t_start_clust;

        auto set_num = 0u;
        for (auto&& cluster : v_clust_temp) {
          clust_location.push_back(set_num);
          ++set_num;
          rec_func(cluster, rec_func);
          clust_location.pop_back();
        }
        cutoffs.pop_back();
      };

      clust_location.push_back(clust_loc_i);
      cluster_rec(clust.second, cluster_rec);
      clust_location.pop_back();
      ++clust_loc_i;
    }

    std::sort(v_clustered.begin(), v_clustered.end(), [](auto a, auto b) {
      return *(a.first.begin()) < *(b.first.begin());
    });

    {
      auto fout = std::ofstream{fout_base + "_clustered"};
      fout << "Number of clusters: " << v_clustered.size() << '\n';
      fout << "--------------------\n";
      num_clusters += v_clustered.size();
      auto clust_loc_pad = std::string::size_type{0};
      auto clust_loc_odd = false;

      for (auto&& cluster_pair : v_clustered) {
        auto clust_loc_len = std::string{};
        for (auto&& pos : cluster_pair.second)
          clust_loc_len += std::to_string(pos) + "-";
        if (clust_loc_len.length() > clust_loc_pad)
          clust_loc_pad = clust_loc_len.length();
      }
      clust_loc_pad += 2;

      for (auto&& cluster_pair : v_clustered) {
        auto clust_loc = std::string{};
        if (clust_loc_odd)
          clust_loc += "B-";
        else
          clust_loc += "A-";
        clust_loc_odd = !clust_loc_odd;
        for (auto&& pos : cluster_pair.second)
          clust_loc += std::to_string(pos) + "-";
        std::ostringstream clust_loc_ss;
        clust_loc_ss << std::left << std::setw(static_cast<int>(clust_loc_pad))
                     << std::setfill('-') << clust_loc << "> ";
        clust_loc = clust_loc_ss.str();
        for (auto&& logli : cluster_pair.first) {
          fout << clust_loc << logli << '\n';
        }
      }
      fout.flush();
    }
    ++proc_num;
  } // Loop over tasks

  auto t_end_tot = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration_tot = t_end_tot - t_start_tot;

  std::cout << "Duration of filters: " << duration_filts.count() << "\n";
  std::cout << "Duration of WED calc: " << duration_wed.count() << "\n";
  std::cout << "Duration of clustering: " << duration_clust.count() << "\n";
  std::cout << "Duration total: " << duration_tot.count() << "\n";
  std::cout << "Parsed log lines: " << std::size(linesvec) << '\n';
  std::cout << "Number of clusters: " << num_clusters << '\n';
}
