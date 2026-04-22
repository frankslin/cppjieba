#ifndef CPPJIEBA_OCD2_LOADER_HPP
#define CPPJIEBA_OCD2_LOADER_HPP

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <marisa.h>

#include "Unicode.hpp"
#include "Trie.hpp"

namespace cppjieba {
namespace detail {

static const char* const OCD2_HEADER = "OPENCC_MARISA_0.2.5";

template <typename IntType>
inline IntType ReadInteger(std::FILE* fp) {
  IntType value = 0;
  if (std::fread(&value, sizeof(IntType), 1, fp) != 1) {
    throw std::runtime_error("failed to read ocd2 integer");
  }
  return value;
}

inline bool EndsWith(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline bool LooksLikeOcd2Dict(const std::string& path) {
  return EndsWith(path, ".ocd2");
}

inline bool ParsePositiveDouble(const std::string& text, double* value) {
  if (text.empty()) {
    return false;
  }
  char* end = NULL;
  const double parsed = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || parsed <= 0.0) {
    return false;
  }
  *value = parsed;
  return true;
}

inline bool ParseNonNegativeDouble(const std::string& text, double* value) {
  if (text.empty()) {
    return false;
  }
  char* end = NULL;
  const double parsed = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || parsed < 0.0) {
    return false;
  }
  *value = parsed;
  return true;
}

struct RawMergedEntry {
  std::string word;
  std::string tag;
  double freq;
  bool contributes_to_base_sum;
  bool uses_default_weight;
  RawMergedEntry(): freq(0.0), contributes_to_base_sum(false), uses_default_weight(false) {
  }
};

inline std::vector<std::vector<std::string> > ReadValues(std::FILE* fp) {
  const uint32_t key_count = ReadInteger<uint32_t>(fp);
  const uint32_t total_length = ReadInteger<uint32_t>(fp);
  std::string buffer(total_length, '\0');
  if (total_length != 0 && std::fread(&buffer[0], sizeof(char), total_length, fp) != total_length) {
    throw std::runtime_error("failed to read ocd2 value buffer");
  }

  const char* cursor = buffer.empty() ? NULL : buffer.data();
  const char* end = buffer.empty() ? NULL : buffer.data() + buffer.size();
  std::vector<std::vector<std::string> > values;
  values.reserve(key_count);

  for (uint32_t i = 0; i < key_count; ++i) {
    const uint16_t value_count = ReadInteger<uint16_t>(fp);
    std::vector<std::string> entry_values;
    entry_values.reserve(value_count);
    for (uint16_t j = 0; j < value_count; ++j) {
      const uint16_t value_size = ReadInteger<uint16_t>(fp);
      if (cursor == NULL || cursor + value_size > end || value_size == 0 || cursor[value_size - 1] != '\0') {
        throw std::runtime_error("invalid ocd2 value payload");
      }
      entry_values.push_back(std::string(cursor, value_size - 1));
      cursor += value_size;
    }
    values.push_back(entry_values);
  }
  return values;
}

template <typename PrecomputedDictType>
inline PrecomputedDictType LoadOcd2Dict(const std::string& path) {
  std::FILE* fp = std::fopen(path.c_str(), "rb");
  if (fp == NULL) {
    throw std::runtime_error("failed to open ocd2 dictionary: " + path);
  }

  try {
    const size_t header_len = std::strlen(OCD2_HEADER);
    std::string header(header_len, '\0');
    if (std::fread(&header[0], sizeof(char), header_len, fp) != header_len) {
      throw std::runtime_error("failed to read ocd2 header");
    }
    if (header != OCD2_HEADER) {
      throw std::runtime_error("invalid ocd2 header");
    }

    marisa::Trie trie;
    marisa::fread(fp, &trie);
    std::vector<std::vector<std::string> > values = ReadValues(fp);
    if (values.size() != trie.num_keys()) {
      throw std::runtime_error("ocd2 payload size does not match trie key count");
    }

    std::vector<RawMergedEntry> entries;
    entries.reserve(trie.num_keys());
    double base_freq_sum = 0.0;
    bool saw_metadata = false;

    for (size_t i = 0; i < trie.num_keys(); ++i) {
      marisa::Agent agent;
      agent.set_query(i);
      trie.reverse_lookup(agent);

      RawMergedEntry raw;
      raw.word.assign(agent.key().ptr(), agent.key().length());
      const std::vector<std::string>& entry_values = values[i];

      if (entry_values.size() == 3) {
        saw_metadata = true;
        raw.tag = entry_values[1];
        if (entry_values[2] == "base") {
          if (!ParsePositiveDouble(entry_values[0], &raw.freq)) {
            throw std::runtime_error("invalid ocd2 base frequency for: " + raw.word);
          }
          raw.contributes_to_base_sum = true;
        } else if (entry_values[2] == "user_freq") {
          if (!ParseNonNegativeDouble(entry_values[0], &raw.freq)) {
            throw std::runtime_error("invalid ocd2 user frequency for: " + raw.word);
          }
          if (raw.freq == 0.0) {
            raw.uses_default_weight = true;
          }
        } else if (entry_values[2] == "user_default") {
          raw.uses_default_weight = true;
        } else {
          throw std::runtime_error("unknown ocd2 entry kind for: " + raw.word);
        }
      } else if (entry_values.size() == 2) {
        raw.tag = entry_values[1];
        if (!ParsePositiveDouble(entry_values[0], &raw.freq)) {
          throw std::runtime_error("invalid legacy ocd2 frequency for: " + raw.word);
        }
        raw.contributes_to_base_sum = true;
      } else if (entry_values.size() == 1) {
        raw.tag = entry_values[0];
        raw.uses_default_weight = true;
      } else {
        throw std::runtime_error("invalid ocd2 values for: " + raw.word);
      }

      if (raw.contributes_to_base_sum) {
        base_freq_sum += raw.freq;
      }
      entries.push_back(raw);
    }

    if (entries.empty()) {
      throw std::runtime_error("ocd2 dictionary is empty");
    }
    if (base_freq_sum <= 0.0) {
      throw std::runtime_error("ocd2 dictionary has no base frequencies");
    }

    std::vector<double> base_weights;
    base_weights.reserve(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      if (entries[i].contributes_to_base_sum) {
        base_weights.push_back(std::log(entries[i].freq / base_freq_sum));
      } else if (!saw_metadata && !entries[i].uses_default_weight && entries[i].freq > 0.0) {
        base_weights.push_back(std::log(entries[i].freq / base_freq_sum));
      }
    }
    if (base_weights.empty()) {
      throw std::runtime_error("ocd2 dictionary has no weights for default entries");
    }

    std::sort(base_weights.begin(), base_weights.end());
    PrecomputedDictType precomputed;
    precomputed.freq_sum = base_freq_sum;
    precomputed.min_weight = base_weights.front();
    precomputed.max_weight = base_weights.back();
    precomputed.median_weight = base_weights[base_weights.size() / 2];
    precomputed.node_infos.reserve(entries.size());

    for (size_t i = 0; i < entries.size(); ++i) {
      DictUnit node_info;
      if (!DecodeUTF8RunesInString(entries[i].word, node_info.word)) {
        throw std::runtime_error("UTF-8 decode failed for ocd2 word: " + entries[i].word);
      }
      node_info.tag = entries[i].tag;
      if (entries[i].uses_default_weight) {
        node_info.weight = precomputed.median_weight;
      } else {
        node_info.weight = std::log(entries[i].freq / base_freq_sum);
      }
      const bool is_user_entry =
          !entries[i].contributes_to_base_sum && (entries[i].uses_default_weight || entries[i].freq >= 0.0);
      if (is_user_entry && node_info.word.size() == 1) {
        precomputed.single_char_user_words.insert(node_info.word[0]);
      }
      precomputed.node_infos.push_back(node_info);
    }

    std::fclose(fp);
    return precomputed;
  } catch (...) {
    std::fclose(fp);
    throw;
  }
}

}  // namespace detail
}  // namespace cppjieba

#endif
