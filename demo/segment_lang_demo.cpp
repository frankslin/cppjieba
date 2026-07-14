// Demo: configure CppJieba to segment both Simplified (SC) and Traditional (TC)
// Chinese.
//
// CppJieba has no "language mode" switch. The script it handles well is decided
// entirely by *which main dictionary you load*:
//   * SC -> the bundled dict/jieba.dict.utf8 (Simplified, MIT).
//   * TC -> third_party/rime-essay's converted dictionary (Traditional, LGPL;
//           generated at build time, see third_party/README.md).
// Everything else (the HMM model for unknown words) is shared. So "configuring"
// for a language is just constructing a segmenter with the right dict path.
//
// This one source builds under both build systems; dictionary paths are
// resolved from CMake's generated test_paths.h or from Bazel runfiles.
//
// Usage:
//   segment_lang_demo                 # segment the built-in SC and TC samples
//   segment_lang_demo sc "简体文本"    # segment custom text with the SC dict
//   segment_lang_demo tc "繁體文字"    # segment custom text with the TC dict
#include "cppjieba/MixSegment.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if defined(__has_include)
#  if __has_include("test_paths.h")
#    include "test_paths.h"
#    define DEMO_CMAKE 1
#  elif __has_include("tools/cpp/runfiles/runfiles.h")
#    include "tools/cpp/runfiles/runfiles.h"
#    define DEMO_BAZEL 1
#  endif
#endif

namespace {

struct DictPaths {
  std::string sc_dict;
  std::string tc_dict;
  std::string hmm_model;
};

#if defined(DEMO_BAZEL)
using bazel::tools::cpp::runfiles::Runfiles;

// Under bzlmod the main repo appears as "_main"; under the legacy WORKSPACE it
// appears as the module name. Try both so the demo is robust either way.
std::string Rlocate(Runfiles* rf, const std::string& rel) {
  const char* prefixes[] = {"_main/", "cppjieba/", ""};
  for (const char* prefix : prefixes) {
    std::string path = rf->Rlocation(std::string(prefix) + rel);
    if (!path.empty() && std::ifstream(path).good()) {
      return path;
    }
  }
  return rel;
}
#endif

bool ResolvePaths(const char* argv0, DictPaths* out) {
#if defined(DEMO_CMAKE)
  (void)argv0;
  out->sc_dict = std::string(DICT_DIR) + "/jieba.dict.utf8";
  out->tc_dict = RIME_ESSAY_DICT_FILE;
  out->hmm_model = std::string(DICT_DIR) + "/hmm_model.utf8";
  return true;
#elif defined(DEMO_BAZEL)
  std::string error;
  Runfiles* rf = Runfiles::Create(argv0, &error);
  if (rf == nullptr) {
    std::cerr << "failed to locate runfiles: " << error << "\n";
    return false;
  }
  out->sc_dict = Rlocate(rf, "dict/jieba.dict.utf8");
  out->tc_dict = Rlocate(rf, "third_party/jieba.rime-essay.dict.utf8");
  out->hmm_model = Rlocate(rf, "dict/hmm_model.utf8");
  return true;
#else
  (void)argv0;
  (void)out;
  std::cerr << "This demo must be built via CMake or Bazel so it can locate "
               "the dictionaries.\n";
  return false;
#endif
}

std::string Join(const std::vector<std::string>& words) {
  std::string joined;
  for (size_t i = 0; i < words.size(); ++i) {
    if (i != 0) joined += " / ";
    joined += words[i];
  }
  return joined;
}

void Segment(cppjieba::MixSegment& seg, const std::string& label,
             const std::string& text) {
  std::vector<std::string> words;
  seg.Cut(text, words);
  std::cout << label << "\n  input : " << text << "\n  cut   : " << Join(words)
            << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  DictPaths paths;
  if (!ResolvePaths(argv[0], &paths)) {
    return 1;
  }

  // Each segmenter is "configured" for a language purely by its main dict.
  cppjieba::MixSegment sc(paths.sc_dict, paths.hmm_model);
  cppjieba::MixSegment tc(paths.tc_dict, paths.hmm_model);

  if (argc >= 3) {
    const std::string lang = argv[1];
    const std::string text = argv[2];
    if (lang == "sc") {
      Segment(sc, "[SC dict]", text);
    } else if (lang == "tc") {
      Segment(tc, "[TC dict]", text);
    } else {
      std::cerr << "usage: " << argv[0] << " [sc|tc] \"text\"\n";
      return 2;
    }
    return 0;
  }

  // Built-in samples: the same sentence in each script, each segmented with the
  // dictionary configured for it.
  std::cout << "== Simplified Chinese (SC dict) ==\n";
  Segment(sc, "[SC dict]", "小明硕士毕业于中国科学院计算所，后在日本京都大学深造");

  std::cout << "\n== Traditional Chinese (TC dict) ==\n";
  Segment(tc, "[TC dict]", "小明碩士畢業於中國科學院計算所，後在日本京都大學深造");

  // Illustrate why the dict matters: run the TC sentence through the SC dict.
  std::cout << "\n== Same TC text, but wrong (SC) dict — note the worse cut ==\n";
  Segment(sc, "[SC dict]", "小明碩士畢業於中國科學院計算所，後在日本京都大學深造");

  return 0;
}
