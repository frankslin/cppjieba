#ifndef CPPJIEBA_TRIE_HPP
#define CPPJIEBA_TRIE_HPP

#include <vector>
#include <algorithm>
#include <string>
#include <utility>
#include "darts.h"
#include "Utils.hpp"
#include "Unicode.hpp"

namespace cppjieba {

using namespace std;

const size_t MAX_WORD_LENGTH = 512;

struct DictUnit {
  Unicode word;
  double weight;
  string tag;
}; // struct DictUnit

// for debugging
// inline ostream & operator << (ostream& os, const DictUnit& unit) {
//   string s;
//   s << unit.word;
//   return os << StringFormat("%s %s %.3lf", s.c_str(), unit.tag.c_str(), unit.weight);
// }

struct Dag {
  RuneStr runestr;
  // [offset, nexts.first]
  LocalVector<pair<size_t, const DictUnit*> > nexts;
  const DictUnit * pInfo;
  double weight;
  size_t nextPos; // TODO
  Dag():runestr(), pInfo(NULL), weight(0.0), nextPos(0) {
  }
}; // struct Dag

typedef Rune TrieKey;

class Trie {
 public:
  Trie(const vector<Unicode>& keys, const vector<const DictUnit*>& valuePointers)
      : value_pointers_(valuePointers) {
    CreateTrie(keys, valuePointers);
  }
  Trie(const vector<string>& encodedKeys, const vector<const DictUnit*>& valuePointers)
      : value_pointers_(valuePointers),
        encoded_keys_(encodedKeys) {
    CreateTrieFromEncoded();
  }
  explicit Trie(const vector<const DictUnit*>& valuePointers)
      : value_pointers_(valuePointers) {
    CreateTrie(valuePointers);
  }
  ~Trie() {
  }

  const DictUnit* Find(RuneStrArray::const_iterator begin, RuneStrArray::const_iterator end) const {
    if (begin == end) {
      return NULL;
    }
    if (encoded_keys_.empty()) {
      return NULL;
    }
    const string encoded = EncodeRunes(begin, end);
    const int result = darts_.exactMatchSearch<int>(encoded.data(), encoded.size());
    if (result < 0 || static_cast<size_t>(result) >= value_pointers_.size()) {
      return NULL;
    }
    return value_pointers_[result];
  }

  const DictUnit* Find(const string& str) const {
    if (str.empty() || encoded_keys_.empty()) {
      return NULL;
    }
    const int result = darts_.exactMatchSearch<int>(str.data(), str.size());
    if (result < 0 || static_cast<size_t>(result) >= value_pointers_.size()) {
      return NULL;
    }
    return value_pointers_[result];
  }

  const DictUnit* Find(const string& sentence,
      RuneStrArray::const_iterator begin,
      RuneStrArray::const_iterator end) const {
    if (begin == end) {
      return NULL;
    }
    const size_t offset = begin->offset;
    const size_t length = (end - 1)->offset - begin->offset + (end - 1)->len;
    return Find(sentence.data() + offset, length);
  }

  void Find(RuneStrArray::const_iterator begin, 
        RuneStrArray::const_iterator end, 
        vector<struct Dag>&res, 
        size_t max_word_len = MAX_WORD_LENGTH) const {
    res.resize(end - begin);
    if (begin == end) {
      return;
    }

    for (size_t i = 0; i < size_t(end - begin); i++) {
      res[i].runestr = *(begin + i);
      res[i].nexts.clear();

      const size_t rune_count = std::min(static_cast<size_t>(end - begin - i), max_word_len);
      if (rune_count == 0 || encoded_keys_.empty()) {
        res[i].nexts.push_back(pair<size_t, const DictUnit*>(i, static_cast<const DictUnit*>(NULL)));
        continue;
      }

      string encoded;
      encoded.reserve(rune_count * 3);
      vector<size_t> rune_end_offsets;
      rune_end_offsets.reserve(rune_count);
      for (size_t j = 0; j < rune_count; ++j) {
        AppendRune(encoded, (begin + i + j)->rune);
        rune_end_offsets.push_back(encoded.size());
      }

      const Darts::DoubleArray::result_pair_type empty_result = {-1, 0};
      vector<Darts::DoubleArray::result_pair_type> matches(rune_count, empty_result);
      const size_t match_count = darts_.commonPrefixSearch(
          encoded.data(),
          matches.data(),
          matches.size(),
          encoded.size());

      bool has_self = false;
      vector<pair<size_t, const DictUnit*> > nexts;
      nexts.reserve(match_count > 0 ? match_count : 1);
      for (size_t k = 0; k < match_count && k < matches.size(); ++k) {
        const Darts::DoubleArray::result_pair_type& match = matches[k];
        if (match.value < 0 || match.length == 0) {
          continue;
        }
        vector<size_t>::const_iterator length_it =
            std::lower_bound(rune_end_offsets.begin(), rune_end_offsets.end(), match.length);
        if (length_it == rune_end_offsets.end() || *length_it != match.length) {
          continue;
        }
        const size_t j = i + static_cast<size_t>(length_it - rune_end_offsets.begin());
        if (j >= size_t(end - begin) || static_cast<size_t>(match.value) >= value_pointers_.size()) {
          continue;
        }
        nexts.push_back(pair<size_t, const DictUnit*>(j, value_pointers_[match.value]));
        if (j == i) {
          has_self = true;
        }
      }

      if (!has_self) {
        res[i].nexts.push_back(pair<size_t, const DictUnit*>(i, static_cast<const DictUnit*>(NULL)));
      }
      for (size_t k = 0; k < nexts.size(); ++k) {
        res[i].nexts.push_back(nexts[k]);
      }
    }
  }

  void Find(const string& sentence,
        RuneStrArray::const_iterator begin,
        RuneStrArray::const_iterator end,
        vector<struct Dag>& res,
        size_t max_word_len = MAX_WORD_LENGTH) const {
    res.resize(end - begin);
    if (begin == end) {
      return;
    }

    for (size_t i = 0; i < size_t(end - begin); ++i) {
      res[i].runestr = *(begin + i);
      res[i].nexts.clear();

      const size_t rune_count = std::min(static_cast<size_t>(end - begin - i), max_word_len);
      if (rune_count == 0 || encoded_keys_.empty()) {
        res[i].nexts.push_back(pair<size_t, const DictUnit*>(i, static_cast<const DictUnit*>(NULL)));
        continue;
      }

      bool has_self = false;
      size_t node_pos = 0;
      for (size_t local_j = 0; local_j < rune_count; ++local_j) {
        const RuneStr& rune = *(begin + i + local_j);
        const char* rune_bytes = sentence.data() + rune.offset;
        int value = -1;
        bool failed = false;
        for (size_t byte_idx = 0; byte_idx < rune.len; ++byte_idx) {
          size_t key_pos = 0;
          value = darts_.traverse(rune_bytes + byte_idx, node_pos, key_pos, 1);
          if (value == -2) {
            failed = true;
            break;
          }
        }
        if (failed) {
          break;
        }

        if (value >= 0 && static_cast<size_t>(value) < value_pointers_.size()) {
          const size_t j = i + local_j;
          res[i].nexts.push_back(pair<size_t, const DictUnit*>(j, value_pointers_[value]));
          if (j == i) {
            has_self = true;
          }
        }
      }

      if (!has_self) {
        res[i].nexts.push_back(pair<size_t, const DictUnit*>(i, static_cast<const DictUnit*>(NULL)));
      }
    }
  }

  void InsertNode(const Unicode& key, const DictUnit* ptValue) {
    (void)key;
    (void)ptValue;
  }
  void DeleteNode(const Unicode& key, const DictUnit* ptValue) {
    (void)key;
    (void)ptValue;
  }
 private:
  void CreateTrie(const vector<Unicode>& keys, const vector<const DictUnit*>& valuePointers) {
    if (valuePointers.empty() || keys.empty()) {
      return;
    }
    assert(keys.size() == valuePointers.size());

    encoded_keys_.clear();
    key_ptrs_.clear();
    key_lengths_.clear();
    values_.clear();
    encoded_keys_.reserve(keys.size());
    values_.reserve(keys.size());
    vector<size_t> order(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
      encoded_keys_.push_back(EncodeUnicode(keys[i]));
      values_.push_back(static_cast<int>(i));
      order[i] = i;
    }
    SortAndBuild(order);
  }

  void CreateTrie(const vector<const DictUnit*>& valuePointers) {
    if (valuePointers.empty()) {
      return;
    }

    encoded_keys_.clear();
    values_.clear();
    encoded_keys_.reserve(valuePointers.size());
    values_.reserve(valuePointers.size());
    vector<size_t> order(valuePointers.size());
    for (size_t i = 0; i < valuePointers.size(); ++i) {
      encoded_keys_.push_back(EncodeUnicode(valuePointers[i]->word));
      values_.push_back(static_cast<int>(i));
      order[i] = i;
    }
    SortAndBuild(order);
  }

  void CreateTrieFromEncoded() {
    if (encoded_keys_.empty()) {
      return;
    }
    values_.clear();
    values_.reserve(encoded_keys_.size());
    vector<size_t> order(encoded_keys_.size());
    for (size_t i = 0; i < encoded_keys_.size(); ++i) {
      values_.push_back(static_cast<int>(i));
      order[i] = i;
    }
    SortAndBuild(order);
  }

  static string EncodeUnicode(const Unicode& unicode) {
    return EncodeRunes(unicode.begin(), unicode.end());
  }

  struct EncodedKeyCompare {
    explicit EncodedKeyCompare(const vector<string>& encoded_keys)
        : encoded_keys_(encoded_keys) {
    }

    bool operator()(size_t lhs, size_t rhs) const {
      return encoded_keys_[lhs] < encoded_keys_[rhs];
    }

   private:
    const vector<string>& encoded_keys_;
  };

  void SortAndBuild(vector<size_t>& order) {
    std::sort(order.begin(), order.end(), EncodedKeyCompare(encoded_keys_));

    vector<string> sorted_keys;
    vector<int> sorted_values;
    sorted_keys.reserve(order.size());
    sorted_values.reserve(order.size());

    for (size_t i = 0; i < order.size(); ++i) {
      const size_t index = order[i];
      if (!sorted_keys.empty() && encoded_keys_[index] == sorted_keys.back()) {
        continue;
      }
      sorted_keys.push_back(std::move(encoded_keys_[index]));
      sorted_values.push_back(values_[index]);
    }
    encoded_keys_.swap(sorted_keys);
    values_.swap(sorted_values);

    key_ptrs_.reserve(encoded_keys_.size());
    key_lengths_.reserve(encoded_keys_.size());
    for (size_t i = 0; i < encoded_keys_.size(); ++i) {
      key_ptrs_.push_back(encoded_keys_[i].data());
      key_lengths_.push_back(encoded_keys_[i].size());
    }

    if (!encoded_keys_.empty()) {
      darts_.build(key_ptrs_.size(), key_ptrs_.data(), key_lengths_.data(), values_.data());
    }
  }

  static string EncodeRunes(RuneStrArray::const_iterator begin, RuneStrArray::const_iterator end) {
    string encoded;
    encoded.reserve((end - begin) * 3);
    for (RuneStrArray::const_iterator it = begin; it != end; ++it) {
      AppendRune(encoded, it->rune);
    }
    return encoded;
  }

  static string EncodeRunes(Unicode::const_iterator begin, Unicode::const_iterator end) {
    string encoded;
    encoded.reserve((end - begin) * 3);
    for (Unicode::const_iterator it = begin; it != end; ++it) {
      AppendRune(encoded, *it);
    }
    return encoded;
  }

  static void AppendRune(string& encoded, Rune rune) {
    if (rune <= 0x7F) {
      encoded.push_back(static_cast<char>(rune));
    } else if (rune <= 0x7FF) {
      encoded.push_back(static_cast<char>(0xC0 | ((rune >> 6) & 0x1F)));
      encoded.push_back(static_cast<char>(0x80 | (rune & 0x3F)));
    } else if (rune <= 0xFFFF) {
      encoded.push_back(static_cast<char>(0xE0 | ((rune >> 12) & 0x0F)));
      encoded.push_back(static_cast<char>(0x80 | ((rune >> 6) & 0x3F)));
      encoded.push_back(static_cast<char>(0x80 | (rune & 0x3F)));
    } else {
      encoded.push_back(static_cast<char>(0xF0 | ((rune >> 18) & 0x07)));
      encoded.push_back(static_cast<char>(0x80 | ((rune >> 12) & 0x3F)));
      encoded.push_back(static_cast<char>(0x80 | ((rune >> 6) & 0x3F)));
      encoded.push_back(static_cast<char>(0x80 | (rune & 0x3F)));
    }
  }

  const DictUnit* Find(const char* data, size_t len) const {
    if (len == 0 || encoded_keys_.empty()) {
      return NULL;
    }
    const int result = darts_.exactMatchSearch<int>(data, len);
    if (result < 0 || static_cast<size_t>(result) >= value_pointers_.size()) {
      return NULL;
    }
    return value_pointers_[result];
  }

  Darts::DoubleArray darts_;
  vector<const DictUnit*> value_pointers_;
  vector<string> encoded_keys_;
  vector<const char*> key_ptrs_;
  vector<size_t> key_lengths_;
  vector<int> values_;
}; // class Trie
} // namespace cppjieba

#endif // CPPJIEBA_TRIE_HPP
