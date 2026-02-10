
#include <cstdint>
#include <libretro.h>
#include <string>
#include <thread>

#include "AudioCommon/AudioCommon.h"
#include "Common/ChunkFile.h"
#include "Common/Event.h"
#include "Common/GL/GLContext.h"
#include "Common/Logging/LogManager.h"
#include "Common/Thread.h"
#include "Common/Version.h"
#include "Core/BootManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/State.h"
#include "Core/System.h"
#include "DolphinLibretro/Audio.h"
#include "DolphinLibretro/Input.h"
#include "DolphinLibretro/Common/Options.h"
#include "DolphinLibretro/Video.h"
#include "VideoBackends/OGL/OGLTexture.h"
#include "VideoBackends/OGL/OGLGfx.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/Widescreen.h"
#include "Core/Boot/Boot.h"

#ifdef PERF_TEST
static struct retro_perf_callback perf_cb;

#define RETRO_PERFORMANCE_INIT(name)                                                               \
  retro_perf_tick_t current_ticks;                                                                 \
  static struct retro_perf_counter name = {#name};                                                 \
  if (!name.registered)                                                                            \
    perf_cb.perf_register(&(name));                                                                \
  current_ticks = name.total

#define RETRO_PERFORMANCE_START(name) perf_cb.perf_start(&(name))
#define RETRO_PERFORMANCE_STOP(name)                                                               \
  perf_cb.perf_stop(&(name));                                                                      \
  current_ticks = name.total - current_ticks;
#else
#define RETRO_PERFORMANCE_INIT(name)
#define RETRO_PERFORMANCE_START(name)
#define RETRO_PERFORMANCE_STOP(name)
#endif

namespace Libretro
{
extern retro_environment_t environ_cb;
static bool widescreen;
}  // namespace Libretro

extern "C" {

void retro_set_environment(retro_environment_t cb)
{
  Libretro::environ_cb = cb;
#ifdef PERF_TEST
  environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb);
#endif
}

void retro_init(void)
{
  enum retro_pixel_format xrgb888 = RETRO_PIXEL_FORMAT_XRGB8888;
  Libretro::environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &xrgb888);
}

void retro_deinit(void)
{
  Libretro::g_emuthread_launched = false;
#ifdef PERF_TEST
  perf_cb.perf_log();
#endif
}

void retro_get_system_info(retro_system_info* info)
{
  info->need_fullpath = true;
  info->valid_extensions = "elf|dol|gcm|iso|tgc|wbfs|ciso|gcz|wad|wia|rvz|m3u";
  info->library_version = Common::GetScmDescStr().c_str();
  info->library_name = "dolphin-emu";
  info->block_extract = true;
}

void retro_get_system_av_info(retro_system_av_info* info)
{
  int efbScale = Libretro::Options::GetCached<int>(
    Libretro::Options::gfx_settings::EFB_SCALE);

  int base_height = EFB_HEIGHT;
  const bool crop_overscan = Libretro::Options::GetCached<bool>(
    Libretro::Options::gfx_settings::CROP_OVERSCAN);

  if (crop_overscan && retro_get_region() == RETRO_REGION_NTSC)
    base_height = 480;

  info->geometry.base_width  = EFB_WIDTH * efbScale;
  info->geometry.base_height = base_height * efbScale;

  info->geometry.max_width   = info->geometry.base_width;
  info->geometry.max_height  = info->geometry.base_height;

  if (g_widescreen)
    Libretro::widescreen = g_widescreen->IsGameWidescreen() || g_Config.bWidescreenHack;
  else if (Core::System::GetInstance().IsWii())
    Libretro::widescreen = Config::Get(Config::SYSCONF_WIDESCREEN);

  info->geometry.aspect_ratio = Libretro::widescreen ? 16.0 / 9.0 : 4.0 / 3.0;
  info->timing.fps = (retro_get_region() == RETRO_REGION_NTSC) ? (60.0f / 1.001f) : 50.0f;
  info->timing.sample_rate = Libretro::Audio::GetActiveSampleRate();
}

void retro_reset(void)
{
  Core::System::GetInstance().GetProcessorInterface().ResetButton_Tap();
}

void retro_run(void)
{
  Libretro::Options::CheckForUpdatedVariables();
  Libretro::FrameTiming::CheckForFastForwarding();
#if defined(_DEBUG)
  Common::Log::LogManager::GetInstance()->SetConfigLogLevel(Common::Log::LogLevel::LDEBUG);
#else
  Common::Log::LogManager::GetInstance()->SetConfigLogLevel(
    static_cast<Common::Log::LogLevel>(
        Libretro::Options::GetCached<int>(
            Libretro::Options::main_interface::LOG_LEVEL, static_cast<int>(Common::Log::LogLevel::LINFO))));
#endif
  double cpuClock = Libretro::Options::GetCached<double>(
    Libretro::Options::core::CPU_CLOCK_RATE);
  Config::SetCurrent(Config::MAIN_OVERCLOCK, cpuClock);
  Config::SetCurrent(Config::MAIN_OVERCLOCK_ENABLE, cpuClock != 1.0);
  g_Config.bWidescreenHack = Libretro::Options::GetCached<bool>(
    Libretro::Options::gfx_settings::WIDESCREEN_HACK);

  const bool crop_overscan = Libretro::Options::GetCached<bool>(
    Libretro::Options::gfx_settings::CROP_OVERSCAN);

  if (crop_overscan && retro_get_region() == RETRO_REGION_NTSC)
    g_Config.bCrop = true;
  else
    g_Config.bCrop = false;

  Libretro::Input::Update();

  Core::System& system = Core::System::GetInstance();

  if (Core::GetState(Core::System::GetInstance()) == Core::State::Starting &&
      !Libretro::g_emuthread_launched)
  {
    WindowSystemInfo wsi(WindowSystemType::Libretro, nullptr, nullptr, nullptr);
    if (system.IsDualCoreMode())
    {
      Core::s_emu_thread = std::thread(Core::EmuThread,
        std::ref(Core::System::GetInstance()), std::move(Core::g_boot_params), wsi);

      // Wait until CPU thread has reached Run()
      auto& cpu_manager = Core::System::GetInstance().GetCPU();
      while (!cpu_manager.HasCPURunStateBeenReached())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    else
      Core::EmuThread(Core::System::GetInstance(), std::move(Core::g_boot_params), wsi);

    Libretro::g_emuthread_launched = true;

    if(Config::Get(Config::MAIN_GFX_BACKEND) == "Software Renderer")
    {
      g_gfx.reset();
      g_gfx = std::make_unique<Libretro::Video::SWGfx>();
    }
    else if (Config::Get(Config::MAIN_GFX_BACKEND) == "Null")
    {
      g_gfx.reset();
      g_gfx = std::make_unique<Libretro::Video::NullGfx>();
    }

    while (!Core::IsRunningOrStarting(Core::System::GetInstance()))
      Common::SleepCurrentThread(100);
  }

  if(!Libretro::g_emuthread_launched)
  {
    DEBUG_LOG_FMT(COMMON, "retro_run() - waiting for g_emuthread_launched");
    return;
  }

  if (g_gfx && Config::Get(Config::MAIN_GFX_BACKEND) == "OGL")
  {
    static_cast<OGL::OGLGfx*>(g_gfx.get())
        ->SetSystemFrameBuffer((GLuint)Libretro::Video::hw_render.get_current_framebuffer());
  }

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_settings::EFB_SCALE))
  {
    g_Config.iEFBScale = Libretro::Options::GetCached<int>(
      Libretro::Options::gfx_settings::EFB_SCALE);

    unsigned cmd = RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO;
    if (Libretro::Video::hw_render.context_type == RETRO_HW_CONTEXT_D3D11 ||
        Libretro::Video::hw_render.context_type == RETRO_HW_CONTEXT_D3D12)
      cmd = RETRO_ENVIRONMENT_SET_GEOMETRY;
    retro_system_av_info info;
    retro_get_system_av_info(&info);
    Libretro::environ_cb(cmd, &info);
  }

  if (g_widescreen &&
      Libretro::widescreen != (g_widescreen->IsGameWidescreen() || g_Config.bWidescreenHack))
  {
    retro_system_av_info info;
    retro_get_system_av_info(&info);
    Libretro::environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &info);
  }

  if (Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_MODE) ||
      Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_OFFSET) ||
      Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_YAW) ||
      Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_PITCH) ||
      Libretro::Options::IsUpdated(Libretro::Options::sysconf::ENABLE_RUMBLE))
  {
    Libretro::Input::ResetControllers();
  }

  if (Libretro::Options::IsUpdated(Libretro::Options::sysconf::WIIMOTE_CONTINUOUS_SCANNING))
  {
    Config::SetCurrent(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING,
      Libretro::Options::GetCached<bool>(Libretro::Options::sysconf::WIIMOTE_CONTINUOUS_SCANNING));
    WiimoteReal::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
  }

  if (Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED))
    Libretro::Input::BluetoothPassthroughBind();

  RETRO_PERFORMANCE_INIT(dolphin_main_func);
  RETRO_PERFORMANCE_START(dolphin_main_func);

  if (system.IsDualCoreMode())
  {
    Core::DoFrameStep(system);
    system.GetFifo().RunGpuLoop();
  }
  else
  {
    system.GetCPU().RunSingleFrame();
  }

  RETRO_PERFORMANCE_STOP(dolphin_main_func);

  if (auto* sound_stream = system.GetSoundStream())
  {
    auto* libretro_stream = static_cast<Libretro::Audio::Stream*>(sound_stream);
    if (libretro_stream)
    {
      libretro_stream->PushAudioForFrame();
    }
  }
}

size_t retro_serialize_size(void)
{
  size_t size = 0;

  Core::System& system = Core::System::GetInstance();
  AsyncRequests* ar = AsyncRequests::GetInstance();

  if (system.IsDualCoreMode())
    ar->SetPassthrough(true);

  Core::RunOnCPUThread(Core::System::GetInstance(), [&] {
    PointerWrap p((u8**)&size, sizeof(size_t), PointerWrap::Mode::Measure);
    State::DoState(Core::System::GetInstance(), p);
    }, true);  // wait = true

  if (system.IsDualCoreMode())
    ar->SetPassthrough(false);

  return size;
}

bool retro_serialize(void* data, size_t size)
{
  Core::System& system = Core::System::GetInstance();
  AsyncRequests* ar = AsyncRequests::GetInstance();

  if (system.IsDualCoreMode())
    ar->SetPassthrough(true);

  Core::RunOnCPUThread(Core::System::GetInstance(), [&] {

    PointerWrap p((u8**)&data, size, PointerWrap::Mode::Write);
    State::DoState(Core::System::GetInstance(), p);
  }, true);

  if (system.IsDualCoreMode())
    ar->SetPassthrough(false);

  return true;
}
bool retro_unserialize(const void* data, size_t size)
{
  Core::System& system = Core::System::GetInstance();
  AsyncRequests* ar = AsyncRequests::GetInstance();

  if (system.IsDualCoreMode())
    ar->SetPassthrough(true);

  Core::RunOnCPUThread(Core::System::GetInstance(), [&] {
    PointerWrap p((u8**)&data, size, PointerWrap::Mode::Read);
    State::DoState(Core::System::GetInstance(), p);
  }, true);

  if (system.IsDualCoreMode())
    ar->SetPassthrough(false);

  return true;
}

unsigned retro_get_region(void)
{
  if (DiscIO::IsNTSC(SConfig::GetInstance().m_region) ||
      (Core::System::GetInstance().IsWii() && Config::Get(Config::SYSCONF_PAL60)))
    return RETRO_REGION_NTSC;

  return RETRO_REGION_PAL;
}

unsigned retro_api_version()
{
  return RETRO_API_VERSION;
}

size_t retro_get_memory_size(unsigned id)
{
  if (id == RETRO_MEMORY_SYSTEM_RAM)
  {
    return Core::System::GetInstance().GetMemory().GetRamSize();
  }
  return 0;
}

void* retro_get_memory_data(unsigned id)
{
  if (id == RETRO_MEMORY_SYSTEM_RAM)
  {
    return Core::System::GetInstance().GetMemory().GetRAM();
  }
  return NULL;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char* code)
{
}
} // extern "C"
