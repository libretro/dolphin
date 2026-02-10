#pragma once

#include <libretro.h>
#include "Common/WindowSystemInfo.h"

namespace Libretro
{
namespace Input
{
void Init(const WindowSystemInfo& wsi);
void InitStage2();
void Update();
void Shutdown();
void ResetControllers();
void BluetoothPassthroughBind();
}
}
