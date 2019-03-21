#!/usr/bin/env bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

uname="$(uname -s)"
case "${uname}" in
    Linux*)     env="linux64"
                GN_SHA1="3523d50538357829725d4ed74b777a572ce0ac74";;
    Darwin*)    env="mac"
                GN_SHA1="d43122f6140d0711518aa909980cb009c4fbce3d";;
esac

echo "Assuming we are running in $env..."

BUILDTOOLS_REPO_URL="https://chromium.googlesource.com/chromium/src/buildtools"
GOOGLE_STORAGE_URL="https://storage.googleapis.com"

GIT_ROOT=$(git rev-parse --show-toplevel)

pushd $GIT_ROOT
set -x  # echo on
curl -Lo gn "$GOOGLE_STORAGE_URL/chromium-gn/$GN_SHA1"
chmod +x gn

sha1=$(curl "$BUILDTOOLS_REPO_URL/+/master/$env/clang-format.sha1?format=TEXT" | base64 --decode)
curl -Lo clang-format "$GOOGLE_STORAGE_URL/chromium-clang-format/$sha1"
chmod +x clang-format
set +x  # echo off

popd
