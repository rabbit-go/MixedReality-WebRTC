# How to build the Chromium WebRTC library

Chromium's libwebrtc library underpins MixedReality-WebRTC. Here are instructions to build the libwebrtc components required by MixedReality-WebRTC.

1. Ensure your working drive has at least 30GB free so it can host the depot_tools and WebRTC repos.

    > NOTE -- This location can be a different drive or folder from your MixedReality-WebRTC repo.

2. A bash shell is required to build libwebrtc.
    * **Linux, Android, Mac, and iOS**: See CONFIG-UNIX.md for instructions to configure your *nix build environment.
    * **Windows, UWP, Hololens**: See CONFIG-WIN.md for instructions to build on Windows.

3. Open a bash shell and change directory to the folder containing this README.md, then do the following:

    1. Run the configuration script: `./config.sh <options>`. This command writes a config file used by subsequent scripts. Options:
        - -h: Show usage.
        - -v: Print all executed commands.
        - -d `WORK_DIR`: Where to put the Chromium depot_tools and clone the WebRTC repo. Can be an absolute or relative path.
        - -b `BRANCH`: The name of the Git branch to clone. E.g.: 'branch-heads/71'
        - -t `TARGET_OS`: Target OS for cross compilation. Default is 'android'. Possible values are: 'linux', 'mac', 'win', 'android', 'ios'.
        - -c `TARGET_CPU`: Target CPU for cross compilation. Default is determined by TARGET_OS. For 'android', it is 'arm64'. Possible values are: 'x86', 'x64', 'arm64', 'arm'.

    2. Run the checkout script: `./checkout.sh`. This command clones the Chromium depot_tools and clones the WebRTC repo. Warning: this can take a long time and consume significant disk space (plan for 5+ hours and 30GB disk space). Options:
        - -h: Show usage.
        - -v: Print all executed commands.

    3. Run the build script: `./build.sh <options>`. This command builds the Chromium WebRTC library. Options:
        - -h: Show usage.
        - -v: Print all executed commands.
        - -c: `BUILD_CONFIG`: Build configuration. Default is 'Release'. Possible values are: 'Debug', 'Release'.

    4. Run the copy script: `./copy.sh`. This command copies libwebrtc build artifacts to the appropriate dependency location for the target platform. Options:
        - -h: Show usage.
        - -v: Print all executed commands.

    5. Build MixedReality-WebRTC.
        - Android: TODO, but basically:
            - Option 1: Open `projects/android` in Android Studio and build.
            - Option 2: Run gradle build from the a shell prompt.
