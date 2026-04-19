#pragma once

#include <libretro.h>
#include <vector>

#include "Core/ActionReplay.h"
#include "Core/GeckoCode.h"
#include "Core/IOS/USB/Emulated/Microphone.h"

namespace Libretro
{
extern retro_environment_t environ_cb;
extern bool g_emuthread_launched;
// Savestate buffered by retro_unserialize() before the EmuThread was alive
extern std::vector<unsigned char> g_pending_load_state;
extern std::vector<Gecko::GeckoCode> g_gecko_codes;
extern std::vector<ActionReplay::ARCode> g_ar_codes;

namespace Input
{
extern retro_microphone_interface g_microphone_interface;
extern bool g_has_microphone_support;
extern bool g_gc_mic_button[4];
extern std::vector<IOS::HLE::USB::Microphone*> g_active_microphones;
} // namespace Input

namespace Video
{
extern retro_video_refresh_t video_cb;
extern struct retro_hw_render_callback hw_render;
}  // namespace Video
}  // namespace Libretro
