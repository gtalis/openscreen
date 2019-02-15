#!/usr/bin/env bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

for f in $(git diff --name-only @{u}); do
  # Skip third party files, except our custom BUILD.gns
  if [[ $f =~ third_party/[^\/]*/src ]]; then
    continue;
  fi

  # Skip non-cpp files
  if ! [[ $f =~ \.(cc|h)$ ]]; then
    continue;
  fi

  # Skip files deleted in this patch
  if ! [[ -f $f ]]; then
    continue;
  fi

  clang-format -style=file -i "$f"
done
