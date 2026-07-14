#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Convert the Rime `essay` word list into a CppJieba main-dictionary.

The Rime essay data (``third_party/rime-essay/essay.txt``) is a two-column,
tab-separated ``词<TAB>词频`` table with no part-of-speech information. CppJieba's
main dictionary (see ``dict/README.md``) requires exactly three space-separated
columns: ``词 词频 词性``. This tool bridges the two formats.

NOTE ON LICENSING: ``essay.txt`` is licensed under the GNU LGPL-3.0 (see
``third_party/rime-essay/LICENSE`` and ``AUTHORS``). Any dictionary produced by
this tool is a derivative work of that data and is therefore **also LGPL-3.0** —
it is NOT covered by CppJieba's MIT license. Keep generated files out of the
MIT-licensed source tree (the default output path stays inside the submodule
directory) and preserve the upstream license/attribution when redistributing.

Usage:
    python3 tools/convert_rime_essay.py \
        --input  third_party/rime-essay/essay.txt \
        --output third_party/rime-essay/jieba.rime-essay.dict.utf8
"""

import argparse
import os
import sys

# CppJieba does not enumerate POS tags; the traditional data has none, so we tag
# every entry with `x` ("非语素字/未知", jieba's conventional catch-all).
DEFAULT_POS = "x"

# essay.txt lists many entries with a frequency of 0. CppJieba turns frequency
# into a log weight, so 0 would blow up. We clamp to this floor by default.
MIN_FREQ = 1


def convert(input_path, output_path, pos, drop_zero, min_freq):
    kept = 0
    dropped = 0
    malformed = 0
    with open(input_path, "r", encoding="utf-8") as fin, \
         open(output_path, "w", encoding="utf-8") as fout:
        for line in fin:
            line = line.rstrip("\n")
            if not line:
                continue
            # essay.txt is tab-separated; fall back to any whitespace just in case.
            parts = line.split("\t")
            if len(parts) < 2:
                parts = line.split()
            if len(parts) < 2:
                malformed += 1
                continue
            word = parts[0].strip()
            freq_raw = parts[1].strip()
            if not word:
                malformed += 1
                continue
            # A word field must not contain spaces (jieba splits on whitespace).
            if " " in word:
                malformed += 1
                continue
            try:
                freq = int(freq_raw)
            except ValueError:
                malformed += 1
                continue
            if freq <= 0:
                if drop_zero:
                    dropped += 1
                    continue
                freq = min_freq
            fout.write("%s %d %s\n" % (word, freq, pos))
            kept += 1
    return kept, dropped, malformed


def main(argv=None):
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    default_in = os.path.join(repo, "third_party", "rime-essay", "essay.txt")
    # Output goes to a gitignored directory (see repo .gitignore). The generated
    # dict is an LGPL derivative, so it must stay out of the MIT repo's history;
    # keeping it here (rather than inside the submodule) also avoids dirtying the
    # submodule working tree.
    default_out = os.path.join(
        repo, "third_party", "generated", "jieba.rime-essay.dict.utf8")

    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--input", default=default_in,
                   help="path to essay.txt (default: %(default)s)")
    p.add_argument("--output", default=default_out,
                   help="output dictionary path (default: %(default)s)")
    p.add_argument("--pos", default=DEFAULT_POS,
                   help="part-of-speech tag for every entry (default: %(default)s)")
    p.add_argument("--drop-zero", action="store_true",
                   help="drop entries with frequency 0 instead of clamping them")
    p.add_argument("--min-freq", type=int, default=MIN_FREQ,
                   help="frequency floor for non-dropped entries (default: %(default)s)")
    args = p.parse_args(argv)

    if not os.path.exists(args.input):
        sys.stderr.write(
            "error: %s not found. Did you run "
            "`git submodule update --init third_party/rime-essay`?\n" % args.input)
        return 1

    out_dir = os.path.dirname(args.output)
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    kept, dropped, malformed = convert(
        args.input, args.output, args.pos, args.drop_zero, args.min_freq)
    sys.stderr.write(
        "wrote %s\n  kept=%d dropped(zero-freq)=%d malformed=%d\n"
        % (args.output, kept, dropped, malformed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
