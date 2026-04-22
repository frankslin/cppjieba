#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <marisa.h>

namespace {

const char* const OCD2_HEADER = "OPENCC_MARISA_0.2.5";

template <typename IntType>
void WriteInteger(FILE* fp, IntType value) {
  const size_t units_written = fwrite(&value, sizeof(IntType), 1, fp);
  if (units_written != 1) {
    throw std::runtime_error("failed to write output file");
  }
}

std::vector<std::string> SplitWhitespace(const std::string& line) {
  std::vector<std::string> tokens;
  std::istringstream iss(line);
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

bool IsCommentOrEmpty(const std::string& line) {
  if (line.empty()) {
    return true;
  }
  for (size_t i = 0; i < line.size(); ++i) {
    unsigned char ch = static_cast<unsigned char>(line[i]);
    if (ch == '#') {
      return true;
    }
    if (!std::isspace(ch)) {
      return false;
    }
  }
  return true;
}

void PrintUsage(std::ostream& os) {
  os << "Usage: cppjieba_dict -i <dict> [-i <dict> ...] -o <output.ocd2>" << std::endl
     << "  -i <dict>         Input dictionary file. The first -i is the base jieba dict;" << std::endl
     << "                    later -i files are parsed as user dict overlays." << std::endl
     << "  -o <output.ocd2>  Output compiled dictionary file." << std::endl;
}

std::vector<std::string> ParseBaseDictValues(const std::vector<std::string>& tokens,
                                             size_t line_number) {
  if (tokens.size() != 3) {
    throw std::runtime_error("invalid base dict line " + std::to_string(line_number));
  }
  return std::vector<std::string>{tokens[1], tokens[2], "base"};
}

std::vector<std::string> ParseUserDictValues(const std::vector<std::string>& tokens,
                                             size_t line_number) {
  if (tokens.empty() || tokens.size() > 3) {
    throw std::runtime_error("invalid user dict line " + std::to_string(line_number));
  }
  if (tokens.size() == 1) {
    return std::vector<std::string>{"", "", "user_default"};
  }
  if (tokens.size() == 2) {
    return std::vector<std::string>{"", tokens[1], "user_default"};
  }
  return std::vector<std::string>{tokens[1], tokens[2], "user_freq"};
}

void LoadBaseDict(const std::string& path, std::map<std::string, std::vector<std::string> >& entries) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) {
    throw std::runtime_error("failed to open base dict: " + path);
  }

  std::string line;
  size_t line_number = 0;
  while (std::getline(ifs, line)) {
    ++line_number;
    if (IsCommentOrEmpty(line)) {
      continue;
    }
    const std::vector<std::string> tokens = SplitWhitespace(line);
    if (tokens.size() < 2) {
      throw std::runtime_error("invalid base dict line " + std::to_string(line_number));
    }
    entries[tokens[0]] = ParseBaseDictValues(tokens, line_number);
  }
}

void LoadUserDict(const std::string& path, std::map<std::string, std::vector<std::string> >& entries) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) {
    throw std::runtime_error("failed to open user dict: " + path);
  }

  std::string line;
  size_t line_number = 0;
  while (std::getline(ifs, line)) {
    ++line_number;
    if (IsCommentOrEmpty(line)) {
      continue;
    }
    const std::vector<std::string> tokens = SplitWhitespace(line);
    if (tokens.empty()) {
      continue;
    }
    entries[tokens[0]] = ParseUserDictValues(tokens, line_number);
  }
}

void SerializeValues(FILE* fp, const std::vector<std::vector<std::string> >& values) {
  std::string value_buffer;
  std::vector<std::vector<uint16_t> > value_sizes;
  uint32_t total_length = 0;

  for (size_t i = 0; i < values.size(); ++i) {
    value_sizes.push_back(std::vector<uint16_t>());
    value_sizes.back().reserve(values[i].size());
    for (size_t j = 0; j < values[i].size(); ++j) {
      total_length += static_cast<uint32_t>(values[i][j].size() + 1);
      value_sizes.back().push_back(static_cast<uint16_t>(values[i][j].size() + 1));
    }
  }

  value_buffer.resize(total_length, '\0');
  char* cursor = value_buffer.empty() ? NULL : &value_buffer[0];
  for (size_t i = 0; i < values.size(); ++i) {
    for (size_t j = 0; j < values[i].size(); ++j) {
      std::strcpy(cursor, values[i][j].c_str());
      cursor += values[i][j].size() + 1;
    }
  }

  WriteInteger<uint32_t>(fp, static_cast<uint32_t>(values.size()));
  WriteInteger<uint32_t>(fp, total_length);
  if (total_length != 0) {
    const size_t bytes_written = fwrite(value_buffer.data(), sizeof(char), total_length, fp);
    if (bytes_written != total_length) {
      throw std::runtime_error("failed to write value buffer");
    }
  }

  for (size_t i = 0; i < values.size(); ++i) {
    WriteInteger<uint16_t>(fp, static_cast<uint16_t>(value_sizes[i].size()));
    for (size_t j = 0; j < value_sizes[i].size(); ++j) {
      WriteInteger<uint16_t>(fp, value_sizes[i][j]);
    }
  }
}

void WriteOcd2(const std::map<std::string, std::vector<std::string> >& entries, const std::string& output_path) {
  marisa::Keyset keyset;
  for (std::map<std::string, std::vector<std::string> >::const_iterator it = entries.begin(); it != entries.end(); ++it) {
    keyset.push_back(it->first.c_str());
  }

  marisa::Trie trie;
  trie.build(keyset);

  std::vector<std::vector<std::string> > values(entries.size());
  for (std::map<std::string, std::vector<std::string> >::const_iterator it = entries.begin(); it != entries.end(); ++it) {
    marisa::Agent agent;
    agent.set_query(it->first.c_str());
    if (!trie.lookup(agent)) {
      throw std::runtime_error("failed to look up built key");
    }
    values[agent.key().id()] = it->second;
  }

  FILE* fp = std::fopen(output_path.c_str(), "wb");
  if (fp == NULL) {
    throw std::runtime_error("failed to open output file: " + output_path);
  }

  try {
    const size_t header_len = std::strlen(OCD2_HEADER);
    const size_t bytes_written = fwrite(OCD2_HEADER, sizeof(char), header_len, fp);
    if (bytes_written != header_len) {
      throw std::runtime_error("failed to write ocd2 header");
    }
    marisa::fwrite(fp, trie);
    SerializeValues(fp, values);
  } catch (...) {
    std::fclose(fp);
    throw;
  }

  std::fclose(fp);
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> input_paths;
  std::string output_path;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-i") {
      if (i + 1 >= argc) {
        std::cerr << "cppjieba_dict: missing value after -i" << std::endl;
        PrintUsage(std::cerr);
        return 1;
      }
      input_paths.push_back(argv[++i]);
      continue;
    }
    if (arg == "-o") {
      if (i + 1 >= argc) {
        std::cerr << "cppjieba_dict: missing value after -o" << std::endl;
        PrintUsage(std::cerr);
        return 1;
      }
      if (!output_path.empty()) {
        std::cerr << "cppjieba_dict: exactly one -o is required" << std::endl;
        PrintUsage(std::cerr);
        return 1;
      }
      output_path = argv[++i];
      continue;
    }

    std::cerr << "cppjieba_dict: unknown argument: " << arg << std::endl;
    PrintUsage(std::cerr);
    return 1;
  }

  if (input_paths.empty()) {
    std::cerr << "cppjieba_dict: at least one -i is required" << std::endl;
    PrintUsage(std::cerr);
    return 1;
  }
  if (output_path.empty()) {
    std::cerr << "cppjieba_dict: exactly one -o is required" << std::endl;
    PrintUsage(std::cerr);
    return 1;
  }

  try {
    std::map<std::string, std::vector<std::string> > entries;
    LoadBaseDict(input_paths.front(), entries);
    for (size_t i = 1; i < input_paths.size(); ++i) {
      LoadUserDict(input_paths[i], entries);
    }
    WriteOcd2(entries, output_path);
    std::cout << "Wrote " << entries.size() << " entries to " << output_path << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "cppjieba_dict: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
