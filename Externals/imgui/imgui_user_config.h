#pragma once

#include "Common/Assert.h"

#define IM_ASSERT(_EXPR) ASSERT(_EXPR)

#define IMGUI_DISABLE_DEMO_WINDOWS

#if defined(__LIBRETRO__) && defined(__SWITCH2__)
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
#endif
