#=============================================================================
# Library functions

#-----------------------------------------------------------------------------
function print-config() {
    echo -e "\e[39mTarget OS: \e[96m$TARGET_OS\e[39m"
    echo -e "\e[39mTarget CPU \e[96m$TARGET_CPU\e[39m"
    echo -e "\e[39mGit Branch: \e[96m$BRANCH\e[39m"
    echo -e "\e[39mWorking dir: \e[96m$WORK_DIR\e[39m"
}

#-----------------------------------------------------------------------------
function read-config() {
    if [ ! -f "$BUILD_DIR/.config.sh" ]; then
        echo -e "\e[39mWebRTC configuration not set.\e[39m"
        echo -e "\e[39mRun ./config.sh first.\e[39m"
        exit 1
    fi
    source "$BUILD_DIR/.config.sh"
    SRC_DIR=$WORK_DIR/webrtc
    # Print a config summary
    echo -e "\e[39m\e[1mActive config:\e[0m\e[39m"
    print-config
}

#-----------------------------------------------------------------------------
function write-config() {
    local filename="$BUILD_DIR/$1"
    cat >$filename <<EOF
TARGET_OS=$TARGET_OS
TARGET_CPU=$TARGET_CPU
BRANCH=$BRANCH
WORK_DIR=$WORK_DIR
EOF
}

#-----------------------------------------------------------------------------
function verify-checkout-config() {
    if [ ! -f "$BUILD_DIR/.config.sh" ]; then
        echo -e "\e[39mWebRTC configuration not set.\e[39m"
        echo -e "\e[39mRun ./config.sh first.\e[39m"
        exit 1
    fi
    if [ ! -f "$BUILD_DIR/.checkout.sh" ]; then
        echo -e "\e[39mWebRTC checkout configuration not set.\e[39m"
        echo -e "\e[39mRun ./checkout.sh first.\e[39m"
        exit 1
    fi
    local config_sh=$(cat "$BUILD_DIR/.config.sh")
    local checkout_sh=$(cat "$BUILD_DIR/.checkout.sh")
    if [ "$checkout_sh" != "$config_sh" ]; then
        echo -e "\e[39mWebRTC configuration has changed since last checkout.\e[39m"
        echo -e "\e[39mRun ./checkout.sh again.\e[39m"
        exit 1
    fi
}

#-----------------------------------------------------------------------------
function write-build-config() {
    local filename="$BUILD_DIR/.build.sh"
    cat >$filename <<EOF
BUILD_CONFIG=$BUILD_CONFIG
EOF
}

#-----------------------------------------------------------------------------
function read-build-config() {
    if [ ! -f "$BUILD_DIR/.build.sh" ]; then
        echo -e "\e[39mWebRTC build configuration not set.\e[39m"
        echo -e "\e[39mRun ./build.sh first.\e[39m"
        exit 1
    fi
    source "$BUILD_DIR/.build.sh"
}

#-----------------------------------------------------------------------------
function detect-host-os() {
    case "$OSTYPE" in
    darwin*) HOST_OS=${HOST_OS:-mac} ;;
    linux*) HOST_OS=${HOST_OS:-linux} ;;
    win32* | msys*) HOST_OS=${HOST_OS:-win} ;;
    *)
        echo -e "\e[91mBuilding on unsupported OS: $OSTYPE\e[39m" && exit 1
        ;;
    esac
}

#-----------------------------------------------------------------------------
function verify-host-os() {
    if [[ $TARGET_OS == 'android' && $HOST_OS != 'linux' ]]; then
        # As per notes at http://webrtc.github.io/webrtc-org/native-code/android/
        echo -e "\e[91mERROR: Android compilation is only supported on Linux.\e[39m"
        exit 1
    fi
}

#-----------------------------------------------------------------------------
function create-WORK_DIR-and-cd() {
    # Ensure directory exists
    mkdir -p $WORK_DIR
    WORK_DIR=$(cd $WORK_DIR && pwd -P)
    # cd to directory
    cd $WORK_DIR
    # mkdir webrtc subdir
    mkdir -p $SRC_DIR
}

#-----------------------------------------------------------------------------
function verify-host-platform-deps() {
    # TODO check for required software packages, environment variables, etc.
    :
}

#-----------------------------------------------------------------------------
function checkout-depot-tools() {
    echo -e "\e[39mCloning depot tools -- this may take some time\e[39m"
    if [ ! -d $DEPOT_TOOLS_DIR ]; then
        git clone -q $DEPOT_TOOLS_URL $DEPOT_TOOLS_DIR
        if [ $HOST_OS = 'win' ]; then
            # run gclient.bat to get python
            pushd $DEPOT_TOOLS_DIR >/dev/null
            ./gclient.bat
            popd >/dev/null
        fi
    else
        pushd $DEPOT_TOOLS_DIR >/dev/null
        git reset --hard -q
        popd >/dev/null
    fi
}

#-----------------------------------------------------------------------------
function checkout-webrtc() {
    echo -e "\e[39mCloning WebRTC source -- this may take a long time\e[39m"
    pushd $SRC_DIR >/dev/null

    # Fetch only the first-time, otherwise sync.
    if [ ! -d src ]; then
        case $TARGET_OS in
        android)
            yes | fetch --nohooks webrtc_android
            ;;
        ios)
            fetch --nohooks webrtc_ios
            ;;
        *)
            fetch --nohooks webrtc
            ;;
        esac
    fi
    # Checkout the specific revision after fetch or for older branches, run
    # setup_links.py to replace all directories which now must be symlinks then
    # try again.
    gclient sync --force --revision $REVISION ||
        (test -f src/setup_links.py && src/setup_links.py --force --no-prompt && gclient sync --force --revision $REVISION)

    popd >/dev/null
}

#-----------------------------------------------------------------------------
function latest-rev() {
    git ls-remote $REPO_URL HEAD | cut -f1
}

#-----------------------------------------------------------------------------
function calc-git-revision() {
    echo -e "\e[39mFetching Git revision\e[39m"
    if [ ! -z $BRANCH ]; then
        REVISION=$(git ls-remote $REPO_URL --heads $BRANCH | head -n1 | cut -f1) ||
            { echo -e "\e[91mCound not get branch revision for $BRANCH\e[39m" && exit 1; }
        [ -z $REVISION ] && echo "Cound not get branch revision for $BRANCH\e[39m" && exit 1
    else
        REVISION=${REVISION:-$(latest-rev $REPO_URL)} ||
            { echo -e "\e[91mCould not get latest revision\e[39m" && exit 1; }
    fi
    echo -e "\e[39mRevision: \e[96m$REVISION\e[39m"
}

#-----------------------------------------------------------------------------
function verify-webrtc-deps() {
    echo -e "\e[39mVerifying WebRTC dependencies\e[39m"
    case $HOST_OS in
    "linux")
        # Automatically accepts ttf-mscorefonts EULA
        echo ttf-mscorefonts-installer msttcorefonts/accepted-mscorefonts-eula select true | sudo debconf-set-selections
        sudo $SRC_DIR/src/build/install-build-deps.sh --no-syms --no-arm --no-chromeos-fonts --no-nacl --no-prompt
        ;;
    esac

    if [ $TARGET_OS = 'android' ]; then
        sudo $SRC_DIR/src/build/install-build-deps-android.sh
    fi
}

#-----------------------------------------------------------------------------
function patch-webrtc-source() {
    echo -e "\e[39mPatching WebRTC source\e[39m"
    pushd $SRC_DIR/src >/dev/null
    # This removes the examples from being built.
    sed -i.bak 's|"//webrtc/examples",|#"//webrtc/examples",|' BUILD.gn
    # This configures to build with RTTI enabled.
    sed -i.bak 's|"//build/config/compiler:no_rtti",|"//build/config/compiler:rtti",|' build/config/BUILDCONFIG.gn
    popd >/dev/null
}

#-----------------------------------------------------------------------------
function configure-build() {
    local config_path="$TARGET_OS/$TARGET_CPU/$BUILD_CONFIG"
    local outdir="out/$config_path"
    echo -e "\e[39mGenerating build configuration\e[39m"
    local args="is_component_build=false rtc_include_tests=false treat_warnings_as_errors=false enable_iterator_debugging=false use_rtti=true"
    [[ "$BUILD_CONFIG" == "Debug" && "$TARGET_OS" == "android" ]] && args+=" android_full_debug=true symbol_level=2"
    [[ "$BUILD_CONFIG" == "Release" ]] && args+=" is_debug=false"
    args+=" target_os=\"$TARGET_OS\" target_cpu=\"$TARGET_CPU\""
    echo -e "\e[90m$args\e[39m"

    pushd "$SRC_DIR/src" >/dev/null
    gn gen $outdir --args="$args"
    popd >/dev/null
}

#-----------------------------------------------------------------------------
function compile-webrtc() {
    local config_path="$TARGET_OS/$TARGET_CPU/$BUILD_CONFIG"
    local outdir="out/$config_path"
    echo -e "\e[39mCompiling WebRTC library\e[39m"

    pushd "$SRC_DIR/src/$outdir" >/dev/null
    ninja -C .
    popd >/dev/null
}

#-----------------------------------------------------------------------------
function package-static-lib-unix() {
    echo -e "\e[39mPackaging WebRTC static library for Unix-like platforms\e[39m"
    local config_path="$TARGET_OS/$TARGET_CPU/$BUILD_CONFIG"
    local outdir="out/$config_path"
    local blacklist="unittest|examples|/yasm|protobuf_lite|main.o|video_capture_external.o|device_info_external.o"

    pushd "$SRC_DIR/src/$outdir" >/dev/null

    rm -f libwebrtc.a
    # Parse .ninja_deps for the names of all the object files.
    local objlist=$(strings .ninja_deps | grep -o '.*\.o')
    # Write all the filenames to a temp file, filtering unwanted entries
    echo "$objlist" | tr ' ' '\n' | grep -v -E $blacklist | grep -v -E '.*clang.*' >libwebrtc.list
    # Additional files needed but not listed in .ninja_deps.
    local extras=$(find \
        ./obj/third_party/boringssl/boringssl_asm -name \
        '*.o')
    # Output extras to the temp file.
    echo "$extras" | tr ' ' '\n' >>libwebrtc.list
    # Generate the static library.
    cat libwebrtc.list | xargs ar -crs libwebrtc.a
    # Add an index to the static library.
    ranlib libwebrtc.a

    popd >/dev/null
}

#-----------------------------------------------------------------------------
function package-static-lib-win() {
    echo -e "\e[39mPackaging WebRTC static library for Windows platform\e[39m"
    local config_path="$TARGET_OS/$TARGET_CPU/$BUILD_CONFIG"
    local outdir="out/$config_path"

    pushd "$SRC_DIR/src/$outdir" >/dev/null

    # TODO
    
    popd >/dev/null
}

#-----------------------------------------------------------------------------
function package-android-archive() {
    echo -e "\e[39mPackaging Android Archive\e[39m"
    local config_path="$TARGET_OS/$TARGET_CPU/$BUILD_CONFIG"
    local outdir="out/$config_path"
    local arch=""
    case "$TARGET_CPU" in
    "arm") arch="armeabi-v7a" ;;
    "arm64") arch="arm64-v8a" ;;
    "x86") arch="x86" ;;
    "x64") arch="x86_64" ;;
    esac

    pushd "$SRC_DIR/src/$outdir" >/dev/null

    [ -f libwebrtc.aar ] && rm -f libwebrtc.aar
    [ -d .aar ] && rm -rf .aar
    mkdir .aar
    mkdir .aar/jni
    mkdir .aar/jni/$arch
    cp libjingle_peerconnection_so.so .aar/jni/$arch
    cp "$SRC_DIR/src/sdk/android/AndroidManifest.xml" .aar/AndroidManifest.xml
    cp lib.java/sdk/android/libwebrtc.jar .aar/classes.jar
    pushd .aar >/dev/null
    zip -r ../libwebrtc.aar *
    popd >/dev/null
    rm -rf .aar

    popd >/dev/null
}

#-----------------------------------------------------------------------------
function package-webrtc() {
    case "$TARGET_OS" in
    "linux" | "android" | "ios" | "mac")
        package-static-lib-unix
        ;;
    "win")
        package-static-lib-win
        ;;
    esac

    case "$TARGET_OS" in
    "android")
        package-android-archive
        ;;
    esac
}
