#!/usr/bin/env bash

#
#    Copyright (c) 2020 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

CHIP_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

APP="$1"
BOARD="$2"
shift 2

if [[ ! -f "$CHIP_ROOT/examples/$APP/nrfconnect/CMakeLists.txt" || -z "$BOARD" ]]; then
    echo "Usage: $0 <application> <board>" >&2
    echo "Applications:" >&2
    ls "$CHIP_ROOT/examples"/*/nrfconnect/CMakeLists.txt | awk -F/ '{print "  "$(NF-2)}' >&2
    exit 1
fi

set -x

# Activate Zephyr environment
[[ -n $ZEPHYR_BASE ]] && source "$ZEPHYR_BASE/../.zephyrrc"

# Activate Matter from repo root so activate.sh submodule checks resolve correctly.
cd "$CHIP_ROOT"
source "$CHIP_ROOT/scripts/activate.sh"
cd "$CHIP_ROOT/examples"

# Set ccache base directory to improve the cache hit ratio
export CCACHE_BASEDIR="$PWD/$APP/nrfconnect"

env
west build -p auto -b "$BOARD" -d "$APP/nrfconnect/build" "$APP/nrfconnect" --sysbuild -- "${COMMON_CI_FLAGS[@]}" "$@"
