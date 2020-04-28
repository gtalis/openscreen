# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script merges code coverage profiles from multiple steps.

This script is taken from the chromium build tools and is synced
manually on an as-needed basis:
https://source.chromium.org/chromium/chromium/src/+/master:testing/merge_scripts/code_coverage/merge_steps.py?q=merge_steps.py&ss=chromium
"""

import argparse
import os
import sys

import merge_lib as merger


def _merge_steps_argument_parser(*args, **kwargs):
  parser = argparse.ArgumentParser(*args, **kwargs)
  parser.add_argument('--input-dir', required=True, help=argparse.SUPPRESS)
  parser.add_argument(
      '--output-file', required=True, help='where to store the merged data')
  parser.add_argument(
      '--llvm-profdata', required=True, help='path to llvm-profdata executable')
  parser.add_argument(
      '--profdata-filename-pattern',
      default='.*',
      help='regex pattern of profdata filename to merge for current test type. '
          'If not present, all profdata files will be merged.')
  return parser


def main():
  desc = "Merge profdata files in <--input-dir> into a single profdata."
  parser = _merge_steps_argument_parser(description=desc)
  params = parser.parse_args()
  merger.merge_profiles(params.input_dir, params.output_file, '.profdata',
                        params.llvm_profdata, params.profdata_filename_pattern)


if __name__ == '__main__':
  sys.exit(main())
