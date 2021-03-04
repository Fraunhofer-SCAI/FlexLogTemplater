#ifndef FLT_TEMPLATING_ONLINE_TEMPLATER_HPP
#define FLT_TEMPLATING_ONLINE_TEMPLATER_HPP

#include <flt/string/word_iterator.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace flt::templating {
inline namespace online {
namespace detail {

class configuration {
  double threshold_;
  bool store_params_;

public:
  configuration(double threshold, bool store_params)
      : threshold_(threshold), store_params_(store_params) {}

  double threshold() const { return threshold_; }
  bool store_params() const { return store_params_; }
};

std::regex special_chars{R"([^a-zA-Z\d:]|\$v)", std::regex::optimize};

template <typename M> auto is_possible_param(const M& token) {
  auto results = std::smatch{};
  return std::regex_search(token, results, special_chars);
}

template <typename M> auto count_tokens(const M& logline) {
  using namespace flt::string::iterator;
  return std::distance(cbegin_wordwise_special_seps(logline),
                       cend_wordwise_special_seps(logline));
}

template <typename M> class templ_node {
  M templ_;
  configuration config_;
  unsigned int templ_length_;
  using token_pos_type = unsigned int;
  std::map<token_pos_type, std::set<M>> params_;

  bool templ_was_split_{false};
  M wildcard_{"<*>"};
  using param_table_entry = std::map<token_pos_type, M>;
  std::set<param_table_entry> param_table_{};
  std::set<token_pos_type> token_positions_with_max_num_tokens_exceeded_{};
  unsigned int max_param_table_entries_{500};

public:
  templ_node(M templ, const configuration& config,
             std::map<unsigned int, std::set<M>> params_to_store =
                 std::map<unsigned int, std::set<M>>{})
      : templ_(templ), config_(config), params_(params_to_store) {
    using namespace flt::string::iterator;
    templ_length_ = 0;
    for (auto it = cbegin_wordwise_special_seps(templ_);
         it != cend_wordwise_special_seps(templ_); ++it)
      ++templ_length_;
  }

  bool templ_was_split() const { return templ_was_split_; }

  void update(const M& logline) {
    auto num_tokens_templ = count_tokens(templ_);
    assert(num_tokens_templ == count_tokens(logline));

    using namespace flt::string::iterator;
    auto logline_it = cbegin_wordwise_special_seps(logline);
    auto logline_it_last = cend_wordwise_special_seps(logline);
    auto templ_it = cbegin_wordwise_special_seps(templ_);
    auto new_templ = M{};
    auto token_pos = 0u;
    auto params_in_templ_replaced = param_table_entry{};
    auto params_in_logline_replaced = param_table_entry{};

    while (logline_it != logline_it_last) {
      if (token_pos == 0)
        new_templ += logline_it.get_prev_seps();
      if (*logline_it != *templ_it && *templ_it != wildcard_) {
        if (token_positions_with_max_num_tokens_exceeded_.find(token_pos) ==
            token_positions_with_max_num_tokens_exceeded_.end()) {
          if (config_.store_params()) {
            params_[token_pos].insert(M(*logline_it));
            params_[token_pos].insert(M(*templ_it));
          }
          params_in_templ_replaced[token_pos] = *templ_it;
          params_in_logline_replaced[token_pos] = *logline_it;
        }
        new_templ += wildcard_;
      } else {
        if (*templ_it == wildcard_) {
          if (token_positions_with_max_num_tokens_exceeded_.find(token_pos) ==
              token_positions_with_max_num_tokens_exceeded_.end()) {
            params_in_logline_replaced[token_pos] = *logline_it;
            if (config_.store_params())
              params_[token_pos].insert(M(*logline_it));
          }
        }
        new_templ += *templ_it;
      }
      new_templ += logline_it.get_next_seps();
      ++logline_it;
      ++templ_it;
      ++token_pos;
    }
    templ_ = new_templ;
    update_param_table(params_in_templ_replaced, params_in_logline_replaced);
  }

  double get_similarity(const M& logline) const {
    auto num_equal_tokens = 0u;
    auto num_tokens_templ = count_tokens(templ_);
    assert(num_tokens_templ == count_tokens(logline));

    using namespace flt::string::iterator;
    auto logline_it = cbegin_wordwise_special_seps(logline);
    auto logline_it_last = cend_wordwise_special_seps(logline);
    auto templ_it = cbegin_wordwise_special_seps(templ_);

    while (logline_it != logline_it_last) {
      if (*logline_it == *templ_it && *logline_it != "&v") {
        ++num_equal_tokens;
      } else if (logline_it.get_prev_seps() != templ_it.get_prev_seps()) {
        return 0.;
      } else if (logline_it.get_next_seps() == "=" &&
                 templ_it.get_next_seps() == "=" &&
                 !is_possible_param(M(*logline_it)) &&
                 !is_possible_param(M(*templ_it))) {
        return 0.;
      }
      ++logline_it;
      ++templ_it;
    }
    if (logline_it.get_next_seps() != templ_it.get_next_seps()) {
      return 0.;
    }
    return double(num_equal_tokens) / num_tokens_templ - config_.threshold();
  }

  void save_templ(std::ofstream& fout, bool save_params = false,
                  M prefix = M{}) const {
    fout << prefix << templ_ << '\n';
    if (save_params) {
      for (const auto& params_at_pos : params_) {
        fout << "  Params at pos " << params_at_pos.first << ':';
        for (const auto& param : params_at_pos.second)
          fout << ' ' << param;
        fout << '\n';
      }
      if (!token_positions_with_max_num_tokens_exceeded_.empty()) {
        fout << "  Positions with max num of tokens exceeded:";
        for (auto pos : token_positions_with_max_num_tokens_exceeded_)
          fout << " " << pos;
      }
    }
  }

  std::set<templ_node> split_templ() {
    templ_was_split_ = false;

    // Step 1: Find token positions with equal numbers of unique tokens
    using num_uniq_toks_type = unsigned int;
    auto token_positions_w_same_num_uniq_toks =
        std::map<num_uniq_toks_type, std::vector<token_pos_type>>{};
    auto token_positions_w_same_num_uniq_params =
        std::map<num_uniq_toks_type, std::vector<token_pos_type>>{};
    auto param_positions = get_param_positions_in_param_table();

    for (auto param_pos : param_positions) {
      auto uniq_toks = std::set<M>{};
      auto param_at_all_pos = true;
      for (auto param_table_entry : param_table_) {
        auto token_at_pos = param_table_entry[param_pos];
        if (param_at_all_pos && !is_possible_param(token_at_pos))
          param_at_all_pos = false;
        uniq_toks.insert(token_at_pos);
      }
      if (!param_at_all_pos)
        token_positions_w_same_num_uniq_toks[uniq_toks.size()].push_back(
            param_pos);
      else
        token_positions_w_same_num_uniq_params[uniq_toks.size()].push_back(
            param_pos);
    }

    // Step 2: Find token positions with bijections

    // Step 2a: Bijections at positions where not all tokens are possible
    // parameters
    auto token_positions_w_bijections = std::set<token_pos_type>{};
    auto token_positions_w_pos1_eq_pos2 = std::set<token_pos_type>{};
    for (auto& token_positions : token_positions_w_same_num_uniq_toks) {
      auto num_uniq_toks = token_positions.second.size();
      if (num_uniq_toks >= 2 && num_uniq_toks <= templ_length_ - 2) {
        for (auto&& pos1 = token_positions.second.begin();
             pos1 != token_positions.second.end(); ++pos1) {
          auto pos1_copy = pos1;
          for (auto&& pos2 = pos1_copy++; pos2 != token_positions.second.end();
               ++pos2) {
            if (pos1 != pos2) {
              auto possible_bijection = true;
              auto par1_par2 = std::map<M, M>{};
              auto par1_eq_par2_for_all_entries = true;
              for (auto param_table_entry : param_table_) {
                if (param_table_entry[*pos1] != param_table_entry[*pos2])
                  par1_eq_par2_for_all_entries = false;
                if (par1_par2.find(param_table_entry[*pos1]) ==
                    par1_par2.end()) {
                  par1_par2[param_table_entry[*pos1]] =
                      param_table_entry[*pos2];
                } else if (par1_par2[param_table_entry[*pos1]] !=
                           param_table_entry[*pos2]) {
                  possible_bijection = false;
                  break;
                }
              }
              if (par1_eq_par2_for_all_entries) {
                token_positions_w_pos1_eq_pos2.insert(*pos1);
                token_positions_w_pos1_eq_pos2.insert(*pos2);
              } else if (possible_bijection) {
                token_positions_w_bijections.insert(*pos1);
                token_positions_w_bijections.insert(*pos2);
              }
            }
          }
        }
      }
    }
    for (const auto& token_pos_w_pos1_eq_pos2 :
         token_positions_w_pos1_eq_pos2) {
      auto token_pos_it =
          token_positions_w_bijections.find(token_pos_w_pos1_eq_pos2);
      if (token_pos_it != token_positions_w_bijections.end())
        token_positions_w_bijections.erase(token_pos_it);
    }

    // Step 2b: Bijections at positions where all tokens are possible parameters
    auto token_positions_w_bijections_add = std::set<token_pos_type>{};
    for (const auto& token_positions : token_positions_w_same_num_uniq_toks) {
      if (token_positions_w_same_num_uniq_params.find(token_positions.first) !=
          token_positions_w_same_num_uniq_params.end()) {
        for (const auto& pos1 : token_positions.second) {
          if (token_positions_w_bijections.find(pos1) !=
              token_positions_w_bijections.end()) {
            for (const auto& pos2 :
                 token_positions_w_same_num_uniq_params[token_positions
                                                            .first]) {
              auto possible_bijection = true;
              auto par1_par2 = std::map<M, M>{};
              for (auto param_table_entry : param_table_) {
                if (par1_par2.find(param_table_entry[pos1]) ==
                    par1_par2.end()) {
                  par1_par2[param_table_entry[pos1]] = param_table_entry[pos2];
                } else if (par1_par2[param_table_entry[pos1]] !=
                           param_table_entry[pos2]) {
                  possible_bijection = false;
                  break;
                }
              }
              if (possible_bijection)
                token_positions_w_bijections_add.insert(pos2);
            }
          }
        }
      }
    }
    token_positions_w_bijections.insert(
        token_positions_w_bijections_add.begin(),
        token_positions_w_bijections_add.end());

    // Step 3: Create new templates
    std::set<templ_node> new_templs{};
    if (!token_positions_w_bijections.empty()) {
      auto new_templ_strings = std::set<M>{};
      auto new_templ_params = std::map<M, decltype(params_)>{};
      for (auto param_tbl_entr : param_table_) {
        auto cur_token_pos = 0u;
        auto new_templ = M{};
        auto params_to_store = std::map<unsigned int, M>{};
        using namespace flt::string::iterator;
        for (auto templ_it = cbegin_wordwise_special_seps(templ_);
             templ_it != cend_wordwise_special_seps(templ_); ++templ_it) {
          if (cur_token_pos == 0)
            new_templ += templ_it.get_prev_seps();
          if (token_positions_w_bijections.find(cur_token_pos) !=
              token_positions_w_bijections.end()) {
            new_templ += param_tbl_entr[cur_token_pos];
          } else {
            new_templ += *templ_it;
            if (config_.store_params() && *templ_it == wildcard_)
              params_to_store[cur_token_pos] = param_tbl_entr[cur_token_pos];
          }
          new_templ += templ_it.get_next_seps();
          ++cur_token_pos;
        }
        new_templ_strings.insert(new_templ);
        if (new_templ_params.find(new_templ) == new_templ_params.end()) {
          for (auto& param : params_to_store) {
            if (token_positions_with_max_num_tokens_exceeded_.find(
                    param.first) !=
                token_positions_with_max_num_tokens_exceeded_.end())
              new_templ_params[new_templ][param.first] = std::set<M>{"*"};
            else
              new_templ_params[new_templ][param.first] =
                  std::set<M>{param.second};
          }
        } else {
          for (const auto& param : params_to_store)
            if (token_positions_with_max_num_tokens_exceeded_.find(
                    param.first) ==
                token_positions_with_max_num_tokens_exceeded_.end())
              new_templ_params[new_templ][param.first].insert(param.second);
        }
      }
      for (auto templ : new_templ_strings)
        new_templs.insert(templ_node{templ, config_, new_templ_params[templ]});
    }
    if (!new_templs.empty())
      templ_was_split_ = true;
    return new_templs;
  }

  bool operator<(const templ_node& other) const {
    return (templ_ < other.templ_);
  }

private:
  std::set<unsigned int> get_param_positions_in_param_table() const {
    std::set<unsigned int> param_positions{};
    auto param_table_entry_first = param_table_.begin();
    if (param_table_entry_first != param_table_.end()) {
      for (auto token_pos : *param_table_entry_first) {
        param_positions.insert(token_pos.first);
      }
    }
    return param_positions;
  }

  void update_param_table(const param_table_entry& templ_params,
                          const param_table_entry& logline_params) {
    if (!templ_params.empty()) {
      if (param_table_.empty()) {
        param_table_.insert(templ_params);
      } else {
        auto updated_param_table = decltype(param_table_){};
        for (auto param_table_entry : param_table_) {
          param_table_entry.insert(templ_params.begin(), templ_params.end());
          updated_param_table.insert(param_table_entry);
        }
        param_table_ = updated_param_table;
      }
    }
    param_table_.insert(logline_params);
    if (param_table_.size() > max_param_table_entries_)
      reduce_param_table();
  }

  void reduce_param_table() {
    auto param_positions = get_param_positions_in_param_table();
    auto uniq_params_at_positions = std::map<token_pos_type, std::set<M>>{};

    for (const auto& param_table_entry : param_table_) {
      for (const auto& param_table_entry_pos : param_table_entry) {
        auto param_pos = param_table_entry_pos.first;
        auto param = param_table_entry_pos.second;
        uniq_params_at_positions[param_pos].insert(param);
      }
    }

    using num_uniq_type = unsigned int;
    auto positions_w_uniq_nums_params =
        std::map<num_uniq_type, std::set<token_pos_type>>{};
    for (const auto& uniq_params_at_pos : uniq_params_at_positions) {
      auto pos = uniq_params_at_pos.first;
      auto num_uniq_params = uniq_params_at_pos.second.size();
      auto positions_w_uniq_num_params =
          positions_w_uniq_nums_params.find(num_uniq_params);
      if (positions_w_uniq_num_params == positions_w_uniq_nums_params.end()) {
        positions_w_uniq_nums_params[num_uniq_params] =
            std::set<token_pos_type>{pos};
      } else {
        positions_w_uniq_num_params->second.insert(pos);
      }
    }

    while (param_table_.size() > max_param_table_entries_) {
      auto param_table_reduced = decltype(param_table_){};
      auto positions_w_largest_num_uniq_toks =
          positions_w_uniq_nums_params.rbegin()->second;
      for (auto param_table_entry : param_table_) {
        for (auto pos_w_largest_num_uniq_toks :
             positions_w_largest_num_uniq_toks) {
          param_table_entry.erase(pos_w_largest_num_uniq_toks);
          token_positions_with_max_num_tokens_exceeded_.insert(
              pos_w_largest_num_uniq_toks);
        }
        param_table_reduced.insert(param_table_entry);
      }
      positions_w_uniq_nums_params.erase(
          positions_w_uniq_nums_params.rbegin()->first);
      param_table_ = param_table_reduced;
    }
  }
};

template <typename M> class templ_layer {
  using node_type = templ_node<M>;
  std::vector<templ_node<M>> nodes;
  std::vector<templ_node<M>> split_nodes;
  configuration config_;

public:
  templ_layer(const configuration& config) : config_(config) {}

  void split_templs() {
    split_nodes = decltype(split_nodes){};
    for (auto& node : nodes) {
      auto new_templs = node.split_templ();
      split_nodes.insert(split_nodes.end(), new_templs.begin(),
                         new_templs.end());
    }
  }

  void print_templs() const {
    for (const auto& node : nodes)
      if (!node.templ_was_split())
        std::cout << "      Template: " << node.templ << '\n';
    for (const auto& node : split_nodes)
      std::cout << "      Template: " << node.templ << '\n';
  }

  void save_templs(std::ofstream& fout, bool save_params = false,
                   const M& prefix = M{}) const {
    for (const auto& node : nodes) {
      if (!node.templ_was_split()) {
        node.save_templ(fout, save_params, prefix);
      }
    }
    for (const auto& node : split_nodes) {
      if (save_params)
        fout << "Split: ";
      node.save_templ(fout, save_params, prefix);
    }
  }

  void update(const M& logline) {
    auto max_similarity = 0.;
    auto cur_similarity = 0.;
    auto node_to_update = decltype(nodes.begin()){};
    for (auto node = nodes.begin(); node != nodes.end(); ++node) {
      cur_similarity = node->get_similarity(logline);
      if (cur_similarity > max_similarity) {
        max_similarity = cur_similarity;
        node_to_update = node;
      }
    }
    if (max_similarity > 0) {
      node_to_update->update(logline);
    } else {
      add_node(logline);
    }
  }

private:
  void add_node(const M& logline) {
    nodes.push_back(templ_node(logline, config_));
  }
};

template <typename M> class token_layer {
  std::unordered_map<M, templ_layer<M>> nodes_first;
  std::unordered_map<M, templ_layer<M>> nodes_last;
  configuration config_;

public:
  token_layer(const configuration& config) : config_(config) {}

  void update(const M& logline) {
    auto& templs_nodes = get_templ_nodes(logline);
    templs_nodes.update(logline);
  }

  void split_templs() {
    for (auto& node : nodes_first)
      node.second.split_templs();
    for (auto& node : nodes_last)
      node.second.split_templs();
  }

  void print_templs() const {
    std::cout << "  Token layer with first token as key\n";
    for (const auto& node : nodes_first) {
      std::cout << "    Token: " << node.first << '\n';
      node.second.print_templs();
    }
    std::cout << "  Token layer with last token as key\n";
    for (const auto& node : nodes_last) {
      std::cout << "    Token: " << node.first << '\n';
      node.second.print_templs();
    }
  }

  void save_templs(std::ofstream& fout, const bool& save_params = false,
                   const M& prefix = M{}) const {
    for (const auto& node : nodes_first) {
      node.second.save_templs(fout, save_params, prefix);
    }
    for (const auto& node : nodes_last) {
      node.second.save_templs(fout, save_params, prefix);
    }
  }

private:
  templ_layer<M>& get_templ_nodes(const M& logline) {
    using namespace flt::string::iterator;
    auto token_it = cbegin_wordwise_special_seps(logline);
    auto first_token = M{*token_it};
    auto first_token_param = is_possible_param(first_token);
    using mapped_type = typename decltype(nodes_first)::mapped_type;
    if (first_token_param) {
      for (int i = 1; i < count_tokens(logline); ++i) {
        token_it++;
      }
      auto last_token = M(*token_it);
      auto last_token_param = is_possible_param(last_token);
      if (last_token_param) {
        nodes_first.insert(std::make_pair("*", mapped_type{config_}));
        return nodes_first.find("*")->second;
      } else {
        nodes_last.insert(std::make_pair(last_token, mapped_type{config_}));
        return nodes_last.find(last_token)->second;
      }
    } else {
      nodes_first.insert(std::make_pair(first_token, mapped_type{config_}));
      return nodes_first.find(first_token)->second;
    }
  }
};

template <typename M> class length_layer {
  configuration config_;
  std::unordered_map<unsigned int, token_layer<M>> nodes;

public:
  length_layer(const configuration& config) : config_{config} {}

  void update(const M& logline) {
    auto num_logline_tokens = count_tokens(logline);
    using mapped_type = typename decltype(nodes)::mapped_type;
    nodes.insert(std::make_pair(num_logline_tokens, mapped_type{config_}));
    nodes.find(num_logline_tokens)->second.update(logline);
  }

  void split_templs() {
    for (auto& node : nodes)
      node.second.split_templs();
  }

  void print_lengths() const {
    for (const auto& node : nodes) {
      std::cout << "Length layer with length = " << node.first << '\n';
      node.second.print_toks();
    }
  }

  void save_lengths(std::ofstream& fout, bool save_params = false,
                    const M& prefix = M{}) const {
    for (const auto& node : nodes) {
      node.second.save_templs(fout, save_params, prefix);
    }
  }
};

} // namespace detail

template <typename M> class online_templater {
  detail::configuration config_;
  detail::length_layer<M> length_nodes;

public:
  online_templater(double threshold = 0.5, bool store_params = false)
      : config_{threshold, store_params}, length_nodes(config_) {}

  void operator()(const M& logline) {
    if (!logline.empty()) {
      length_nodes.update(logline);
    }
  }

  auto split_templs() { length_nodes.split_templs(); }

  auto print_templs() const { length_nodes.print_lengths(); }

  auto save_templs(std::ofstream& fout, bool save_params = false,
                   const M prefix = M{}) const {
    length_nodes.save_lengths(fout, save_params, prefix);
  }
};

} // namespace online
} // namespace flt::templating

#endif
