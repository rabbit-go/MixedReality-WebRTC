// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "targetver.h"

#if defined(MR_SHARING_WIN)

#define WEBRTC_WIN 1

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#else defined(MR_SHARING_ANDROID)

#define WEBRTC_POSIX 1

#endif

// Prevent external headers from triggering warnings that would break compiling
// due to warning-as-error.
#pragma warning(push, 2)
#pragma warning(disable : 4100)
#pragma warning(disable : 4244)

// UWP wrappers
#if defined(WINUWP)
#include <winrt/windows.applicationmodel.core.h>
#endif

#pragma warning(pop)
