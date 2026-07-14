// Smoke test: load the LGPL rime-essay traditional dictionary and confirm
// CppJieba segments traditional Chinese with it. The dictionary is generated at
// build time from third_party/rime-essay/essay.txt and consumed as data.
//
// Works under both build systems:
//   * CMake  - paths come from the generated test_paths.h.
//   * Bazel  - paths are resolved from runfiles (data dependency).
#include "cppjieba/MixSegment.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#if defined(__has_include)
#  if __has_include("test_paths.h")
#    include "test_paths.h"
#    define CPPJIEBA_HAVE_TEST_PATHS 1
#  endif
#endif

namespace {

#ifndef CPPJIEBA_HAVE_TEST_PATHS
std::string RunfilePath(const std::string& relative_path) {
  const char* test_srcdir = std::getenv("TEST_SRCDIR");
  const char* test_workspace = std::getenv("TEST_WORKSPACE");
  if (test_srcdir == NULL || test_workspace == NULL) {
    return relative_path;
  }
  return std::string(test_srcdir) + "/" + test_workspace + "/" + relative_path;
}
#endif

}  // namespace

int main() {
#ifdef CPPJIEBA_HAVE_TEST_PATHS
  const std::string dict_path = RIME_ESSAY_DICT_FILE;
  const std::string model_path = std::string(DICT_DIR) + "/hmm_model.utf8";
#else
  const std::string dict_path =
      RunfilePath("third_party/jieba.rime-essay.dict.utf8");
  const std::string model_path = RunfilePath("dict/hmm_model.utf8");
#endif

  cppjieba::MixSegment seg(dict_path, model_path);

  std::vector<std::string> words;
  seg.Cut("清華大學讀書並且喜歡自然語言處理", words);
  if (words.empty()) {
    std::cerr << "Expected the rime-essay dictionary to segment the sentence.\n";
    return 1;
  }

  // The traditional dictionary should recognise the multi-character term
  // "自然語言處理" as a single token, which the simplified default dict does not.
  for (const std::string& w : words) {
    if (w == "自然語言處理") {
      return 0;
    }
  }

  std::cerr << "Segmentation did not yield the expected traditional term. Got:";
  for (const std::string& w : words) {
    std::cerr << " " << w;
  }
  std::cerr << "\n";
  return 1;
}
