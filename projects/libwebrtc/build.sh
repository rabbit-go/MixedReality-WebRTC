#!/bin/bash

#=============================================================================
# Global config

set -o errexit
set -o nounset
set -o pipefail

BUILD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

source "$BUILD_DIR/lib.sh"

#-----------------------------------------------------------------------------
function usage() {
    cat <<EOF
Usage:
    $0 [OPTIONS]

Builds Chromium's WebRTC component.

OPTIONS:
    -h              Show this message.
    -v              Verbose mode. Print all executed commands.
    -c BUILD_CONFIG Build configuration. Default is 'Release'. Possible values are: 'Debug', 'Release'.
EOF
}

#-----------------------------------------------------------------------------
function verify-arguments() {
    VERBOSE=${VERBOSE:-0}
    BUILD_CONFIG=${BUILD_CONFIG:-Release}
    DEPOT_TOOLS_DIR=$WORK_DIR/depot_tools
    PATH=$DEPOT_TOOLS_DIR:$DEPOT_TOOLS_DIR/python276_bin:$PATH
    # Print all executed commands?
    [ "$VERBOSE" = 1 ] && set -x
    echo -e "\e[39mBuild configuration: \e[96m$BUILD_CONFIG\e[39m"
    # Verify build config
    if [[ "$BUILD_CONFIG" != "Debug" && "$BUILD_CONFIG" != "Release" ]]; then
        echo -e "\e[39mUnknown build configuration: \e[91m$BUILD_CONFIG\e[39m"
        exit 1
    fi
}

#-----------------------------------------------------------------------------
function print-summary() {
    local config_path="$TARGET_OS/$TARGET_CPU/$BUILD_CONFIG"
    local outdir_full="$SRC_DIR/src/out/$config_path"

    case $TARGET_OS in
    "android")
        echo -e "\e[39mStatic library : \e[96m$outdir_full/libwebrtc.a\e[39m"
        echo -e "\e[39mAndroid Archive: \e[96m$outdir_full/libwebrtc.aar\e[39m"
        echo -e "\e[39mHeader path    : \e[96m$SRC_DIR\e[39m"
        ;;
    *)
        echo "TODO: build summary for this platform"
        ;;
    esac
}

#=============================================================================
# Main

# Read command line
while getopts c:v OPTION; do
    case ${OPTION} in
    c) BUILD_CONFIG=$OPTARG ;;
    v) VERBOSE=1 ;;
    ?) usage && exit 1 ;;
    esac
done

# Read config from file
read-config

# Verify source is checked out and configs match
verify-checkout-config

# Ensure all arguments have reasonable values
verify-arguments

# Compile and package webrtc library
echo -e "\e[39mBuilding: \e[96m$TARGET_OS/$TARGET_CPU/$BUILD_CONFIG\e[39m"
#configure-build
#compile-webrtc
#package-webrtc
print-summary

write-build-config

echo -e "\e[39m\e[1mComplete.\e[0m\e[39m"
echo -e "\e[39m\e[1mNext step:\e[0m run ./copy.sh\e[39m"
