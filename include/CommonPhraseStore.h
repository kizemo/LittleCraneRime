#pragma once
#include <WeaselUtility.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace weasel {
namespace common_phrase {

inline std::filesystem::path PhraseFilePath() {
  return WeaselUserDataPath() / L"weasel_common_phrases.txt";
}

inline bool LoadPhrases(std::vector<std::wstring>& out) {
  out.clear();
  std::filesystem::path path = PhraseFilePath();
  if (!std::filesystem::exists(path))
    return true;
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line[0] == '\xEF' && line.size() >= 3) {
      if (static_cast<unsigned char>(line[1]) == 0xBB &&
          static_cast<unsigned char>(line[2]) == 0xBF)
        line.erase(0, 3);
    }
    if (line.empty() || line[0] == '#')
      continue;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
      line.pop_back();
    if (!line.empty())
      out.push_back(u8tow(line));
  }
  return true;
}

inline bool SavePhrases(const std::vector<std::wstring>& phrases) {
  std::filesystem::path path = PhraseFilePath();
  if (!std::filesystem::exists(path.parent_path()))
    std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
    return false;
  out << "# 常用短语（每行一条，无需编码）\n# encoding: utf-8\n\n";
  for (const auto& phrase : phrases)
    out << wtou8(phrase) << "\n";
  return true;
}

}  // namespace common_phrase
}  // namespace weasel
