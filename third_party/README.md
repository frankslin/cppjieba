# 第三方组件与许可证 (Third-Party Components & Licensing)

CppJieba 本体以 **MIT** 许可证发布。本目录下的组件**各自保留其上游许可证**，
并**不受** CppJieba 的 MIT 许可证覆盖。集成方在再分发时必须遵守对应许可证。

## rime-essay （繁体主词典数据源）

- **路径**: `third_party/rime-essay/`（Git submodule，指向
  <https://github.com/frankslin/rime-essay>）。
- **内容**: `essay.txt` —— Rime 输入法预设词汇表，两列制表符分隔
  `词<TAB>词频`，以繁体中文为主，无词性信息。
- **许可证**: **GNU LGPL-3.0**（见 `rime-essay/LICENSE`）。
- **上游来源与归属**（见 `rime-essay/AUTHORS`）:
  - Chewing / 新酷音 (LGPL)
  - opencc / 開放中文轉換 (Apache-2.0)
  - Android Pinyin IME (Apache-2.0)
  - moedict.tw／萌典 (CC0 1.0)

  该数据为上述来源的再加工合集，整体以 LGPL-3.0 授权。

### 重要合规说明

- `essay.txt` 及**任何由它转换得到的词典**（例如
  `tools/convert_rime_essay.py` 生成的 `jieba.rime-essay.dict.utf8`）都是
  **LGPL-3.0 的衍生作品**，**不是 MIT**。
- 该数据作为**运行时加载的数据文件**使用，不会被编译进二进制；用户可以自由
  替换该词典文件，天然满足 LGPL 对“可替换/可修改”组件的要求。
- 转换过程只产出**构建产物**，不进入 CppJieba（MIT）主仓库的 Git 历史，也不会
  弄脏 submodule 工作树：
  - **Bazel**：`//third_party:rime_essay_dict` genrule，输出仅在 `bazel-out/`；
    cppjieba 通过 `data` 消费（`//tools:convert_rime_essay.py` 是 MIT 工具）。
  - **非 Bazel**：`tools/convert_rime_essay.py` 默认写到 gitignored 的
    `third_party/generated/`（见根 `.gitignore`）。
  - 请勿将转换产物复制进 MIT 许可的源码树中再分发。
- 再分发包含此数据的产物时，必须一并保留 `rime-essay/LICENSE` 与
  `rime-essay/AUTHORS`。

## 使用方式

见仓库根 `README.md` 的「繁体词典（可选，LGPL）」一节。
