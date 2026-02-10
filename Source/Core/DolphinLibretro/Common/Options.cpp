
#include <libretro.h>
#include <cstdlib>
#include "DolphinLibretro/Common/Options.h"

namespace Libretro
{
namespace Options
{
static std::unordered_map<std::string, std::string> optionCache;
static std::unordered_map<std::string, bool> optionDirty;

// Category key constants
static constexpr const char* CATEGORY_CORE = "core";
static constexpr const char* CATEGORY_AUDIO = "audio";
static constexpr const char* CATEGORY_INTERFACE = "interface";
static constexpr const char* CATEGORY_SYSCONF = "sysconf";
static constexpr const char* CATEGORY_GFX_HARDWARE = "graphics_hardware";
static constexpr const char* CATEGORY_GFX_SETTINGS = "graphics_settings";
static constexpr const char* CATEGORY_GFX_ENHANCEMENTS = "graphics_enhancements";
static constexpr const char* CATEGORY_GFX_HACKS = "graphics_hacks";
static constexpr const char* CATEGORY_WIIMOTE = "wiimote";

// V2 Categories
static const struct retro_core_option_v2_category option_cats[] = {
  {
    CATEGORY_CORE,
    "Core",
    "Configure CPU emulation, timing, and core system settings."
  },
  {
    CATEGORY_AUDIO,
    "Audio / DSP",
    "Configure audio output and DSP emulation."
  },
  {
    CATEGORY_INTERFACE,
    "Interface",
    "Configure on-screen display and logging."
  },
  {
    CATEGORY_SYSCONF,
    "System Configuration",
    "Configure Wii system settings."
  },
  {
    CATEGORY_GFX_HARDWARE,
    "Graphics > Hardware",
    "Configure graphics hardware options."
  },
  {
    CATEGORY_GFX_SETTINGS,
    "Graphics > Settings",
    "Configure rendering backend and quality settings."
  },
  {
    CATEGORY_GFX_ENHANCEMENTS,
    "Graphics > Enhancements",
    "Configure texture filtering and visual enhancements."
  },
  {
    CATEGORY_GFX_HACKS,
    "Graphics > Hacks",
    "Configure accuracy vs performance tradeoffs."
  },
  {
    CATEGORY_WIIMOTE,
    "Wiimote IR",
    "Configure Wiimote infrared pointer settings."
  },
  { NULL, NULL, NULL }
};

#if defined(_M_X86_64)
  #define CPU_CORE_DEFAULT "1" // PowerPC::CPUCore::JIT64
  #define CPU_CORE_JIT_ENTRY { "1", "JIT64 (Recommended)" },
#elif defined(_M_ARM_32)
  #define CPU_CORE_DEFAULT "5"  // PowerPC::CPUCore::CachedInterpreter
  #define CPU_CORE_JIT_ENTRY
//  #define CPU_CORE_JIT_ENTRY { "3", "JITARM (Recommended)" }, // PowerPC::CPUCore::JITARM
#elif defined(_M_ARM_64)
  #define CPU_CORE_DEFAULT "4" // PowerPC::CPUCore::JITARM64
  #define CPU_CORE_JIT_ENTRY { "4", "JITARM64 (Recommended)" },
#else
  #define CPU_CORE_DEFAULT "5" // PowerPC::CPUCore::CachedInterpreter
  #define CPU_CORE_JIT_ENTRY
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

static struct retro_core_option_v2_definition option_defs[] = {

  // ========== Main.Core ==========
  {
    Libretro::Options::core::CPU_CORE,
    "Core > CPU Core",
    "CPU Core",
    "Select CPU emulation method - JIT provides best performance.",
    nullptr,
    CATEGORY_CORE,
    {
      { "0", "Interpreter (slowest)" },
      CPU_CORE_JIT_ENTRY
      { "5", "Cached Interpreter (slower)" },
      { nullptr, nullptr }
    },
    CPU_CORE_DEFAULT
  },
  {
    Libretro::Options::core::CPU_CLOCK_RATE,
    "Core > CPU Clock Rate",
    "CPU Clock Rate",
    "Adjust emulated CPU speed.",
    nullptr,
    CATEGORY_CORE,
    {
      { "0.05", "5%"  }, { "0.10", "10%" }, { "0.20", "20%" }, { "0.30", "30%" },
      { "0.40", "40%" }, { "0.50", "50%" }, { "0.60", "60%" }, { "0.70", "70%" },
      { "0.80", "80%" }, { "0.90", "90%" }, { "1.00", "100% (Default)" },
      { "1.50", "150%" }, { "2.00", "200%" }, { "2.50", "250%" }, { "3.00", "300%" },
      { nullptr, nullptr }
    },
    "1.00"
  },
  {
    Libretro::Options::core::EMULATION_SPEED,
    "Core > Emulation Speed",
    "Emulation Speed",
    "Set speed limit for emulation.",
    nullptr,
    CATEGORY_CORE,
    {
      { "1.0", "100% (Normal Speed)" },
      { "0.0", "Unlimited" },
      { nullptr, nullptr }
    },
    "0.0"
  },
  {
    Libretro::Options::core::MAIN_CPU_THREAD,
    "Core > Dual Core Mode",
    "Dual Core Mode",
    "Enable dual-core CPU emulation. Requires core RESTART.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", "Disabled" },
      { "enabled",  "Enabled" },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::core::MAIN_PRECISION_FRAME_TIMING,
    "Core > Precision Frame Timing",
    "Precision Frame Timing",
    "Use busy-wait for more accurate frame timing.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
//#if defined(ANDROID)
    "disabled"
//#else
//    "enabled"
//#endif
  },
  {
    Libretro::Options::core::FASTMEM,
    "Core > Fastmem",
    "Fastmem",
    "Enable fastmem optimization - which uses memory mapping for faster access.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::core::FASTMEM_ARENA,
    "Core > Fastmem Arena",
    "Fastmem Arena",
    "Enable fastmem arena - reserves 12 GiB of virtual memory for super fast access.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::core::MAIN_ACCURATE_CPU_CACHE,
    "Core > Accurate CPU cache",
    "Accurate CPU cache",
    "Enabled - fast, Disabled - guarantees correctness involving cache behaviour.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::core::CHEATS_ENABLED,
    "Core > Internal Cheats",
    "Internal Cheats",
    "Enable built-in cheat codes.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::core::SKIP_GC_BIOS,
    "Core > Skip GameCube BIOS",
    "Skip GameCube BIOS",
    "Skip the GameCube BIOS animation/menu and start the game directly.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::core::LANGUAGE,
    "Core > System Language",
    "System Language",
    "Set system language.",
    nullptr,
    CATEGORY_CORE,
    {
      { "1", "English" }, // DiscIO::Language::English
      { "0", "Japanese" }, // DiscIO::Language::Japanese
      { "2", "German" }, // DiscIO::Language::German
      { "3", "French" }, // DiscIO::Language::French
      { "4", "Spanish" }, // DiscIO::Language::Spanish
      { "5", "Italian" }, // DiscIO::Language::Italian
      { "6", "Dutch" }, // DiscIO::Language::Dutch
      { "7", "Simplified Chinese" }, // DiscIO::Language::SimplifiedChinese
      { "8", "Traditional Chinese" }, // DiscIO::Language::TraditionalChinese
      { "9", "Korean" }, // DiscIO::Language::Korean
      { nullptr, nullptr }
    },
    "1"
  },
  {
    Libretro::Options::core::FAST_DISC_SPEED,
    "Core > Speed Up Disc Transfer",
    "Speed Up Disc Transfer",
    "Reduce loading times.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::core::MAIN_MMU,
    "Core > Enable MMU",
    "Enable MMU",
    "Enable emulation of the Memory Management Unit.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::core::RUSH_FRAME_PRESENTATION,
    "Core > Rush Frame Presentation",
    "Rush Frame Presentation",
    "Enable rushing frame presentation for lower latency.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::core::SMOOTH_EARLY_PRESENTATION,
    "Core > Smooth Early Presentation",
    "Smooth Early Presentation",
    "Enable smoother early frame presentation timing.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },

  // ========== Main.Interface ==========
  {
    Libretro::Options::main_interface::OSD_ENABLED,
    "Interface > On-Screen Display",
    "On-Screen Display",
    "Show OSD messages.",
    nullptr,
    CATEGORY_INTERFACE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::main_interface::LOG_LEVEL,
    "Interface > Log Level",
    "Log Level",
    "Set log verbosity.",
    nullptr,
    CATEGORY_INTERFACE,
    {
      { "1", "Notice" },  // VERY important information that is NOT errors. Like startup and OSReports.
      { "2", "Error" },   // Critical errors
      { "3", "Warning" }, // Something is suspicious.
      { "4", "Info" },    // General information
#if defined(_DEBUG) || defined(DEBUGFAST)
      { "5", "Debug" },   // Detailed debugging - might make things slow
#endif
      { nullptr,   nullptr }
    },
    "Info"
  },
  {
    Libretro::Options::main_interface::ENABLE_DEBUGGING,
    "Interface > Enable debugging",
    "Enable debugging",
    "Enable the debugger.",
    nullptr,
    CATEGORY_INTERFACE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },

  // ========== Main.DSP ==========
  {
    Libretro::Options::audio::DSP_HLE,
    "Audio / DSP > DSP HLE",
    "DSP HLE",
    "Choose DSP method - HLE is faster, LLE is more accurate.",
    nullptr,
    CATEGORY_AUDIO,
    {
      { "enabled",  "HLE (Fast)" },
      { "disabled", "LLE (Accurate)" },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::audio::DSP_JIT,
    "Audio / DSP > DSP JIT",
    "DSP JIT",
    "Enable JIT for DSP LLE.",
    nullptr,
    CATEGORY_AUDIO,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::audio::CALL_BACK_AUDIO,
    "Audio / DSP > Async Audio Callback",
    "Async Audio Callback",
    "Use asynchronous audio callbacks.",
    "Pushes audio asynchronously instead of synchronously. Restart core to take affect.",
    CATEGORY_AUDIO,
    {
      { "0", "Sync - Dolphin will push samples" },
      { "1",  "Sync - Per Frame using target refresh rate" },
      { "2",  "Async - driven by callbacks using refresh rate and audio buffer status" },
      { nullptr, nullptr }
    },
    "0"
  },

  // ========== SYSCONF.IPL ==========
  {
    Libretro::Options::sysconf::WIDESCREEN,
    "System Configuration > Widescreen (Wii)",
    "Widescreen (Wii)",
    "Enable widescreen for Wii.",
    nullptr,
    CATEGORY_SYSCONF,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::sysconf::PROGRESSIVE_SCAN,
    "System Configuration > Progressive Scan",
    "Progressive Scan",
    "Enable progressive scan.",
    nullptr,
    CATEGORY_SYSCONF,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::sysconf::PAL60,
    "System Configuration > PAL60 Mode",
    "PAL60 Mode",
    "Enable 60Hz for PAL games.",
    nullptr,
    CATEGORY_SYSCONF,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },

  // ========== SYSCONF.BT ==========
  {
    Libretro::Options::sysconf::SENSOR_BAR_POSITION,
    "System Configuration > Sensor Bar Position",
    "Sensor Bar Position",
    "Set Wiimote sensor bar position.",
    nullptr,
    CATEGORY_SYSCONF,
    {
      { "0", "Bottom" },
      { "1", "Top" },
      { nullptr, nullptr }
    },
    "0"
  },
  {
    Libretro::Options::sysconf::ENABLE_RUMBLE,
    "System Configuration > Controller Rumble",
    "Controller Rumble",
    "Enable rumble feedback.",
    nullptr,
    CATEGORY_SYSCONF,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::sysconf::WIIMOTE_CONTINUOUS_SCANNING,
    "System Configuration > Wiimote Continuous Scanning",
    "Wiimote Continuous Scanning",
    "Continuously scan for Wiimotes.",
    nullptr,
    CATEGORY_SYSCONF,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::sysconf::ALT_GC_PORTS_ON_WII,
    "System Configuration > Alt GC Ports (Wii)",
    "Alt GC Ports (Wii)",
    "Use ports 5-8 for GameCube controllers in Wii mode.",
    nullptr,
    CATEGORY_SYSCONF,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },

  // Main.BluetoothPassthrough
  {
    Libretro::Options::main_bluetooth::BLUETOOTH_PASSTHROUGH,
    "System Configuration > Bluetooth passthrough mode",
    "Bluetooth passthrough mode",
    "Pass all traffic directly to the host's Bluetooth adapter. Press F12 to begin the sync process.",
    nullptr,
    CATEGORY_SYSCONF,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },

  // ========== Graphics.Hardware ==========
  /*{
    Libretro::Options::gfx_hardware::VSYNC,
    "Graphics > Hardware > VSync",
    "VSync",
    "Sync with display refresh.",
    nullptr,
    CATEGORY_GFX_HARDWARE,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },*/

  // ========== Graphics.Settings ==========
  {
    Libretro::Options::gfx_settings::RENDERER,
    "Graphics > Settings > Graphics Backend",
    "Graphics Backend",
    "Select rendering backend.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "Hardware", "Hardware" },
  #if defined(_DEBUG) || defined(DEBUGFAST)
      { "Software", "Software Renderer" },
      { "Null",     "Null Renderer" },
  #endif
      { nullptr, nullptr }
    },
    "Hardware"
  },
  {
    Libretro::Options::gfx_settings::WIDESCREEN_HACK,
    "Graphics > Settings > Widescreen Hack",
    "Widescreen Hack",
    "Force 16:9 rendering.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_settings::CROP_OVERSCAN,
    "Graphics > Settings > Crop Overscan",
    "Crop Overscan",
    "Crop overscan to match standard NTSC output resolutions. Recommended for NTSC CRTs.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_settings::EFB_SCALE,
    "Graphics > Settings > Internal Resolution",
    "Internal Resolution",
    "Multiply native resolution.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "1", "1x Native (640x528)" },
      { "2", "2x Native (1280x1056) for 720p" },
      { "3", "3x Native (1920x1584) for 1080p" },
      { "4", "4x Native (2560x2112) for 1440p" },
      { "5", "5x Native (3200x2640)" },
      { "6", "6x Native (3840x3168) for 4K" },
      { nullptr, nullptr }
    },
    "1"
  },
  {
    Libretro::Options::gfx_settings::SHADER_COMPILATION_MODE,
    "Graphics > Settings > Shader Compilation",
    "Shader Compilation",
    "Control shader compilation.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "0", "Synchronous" }, // ShaderCompilationMode::Synchronous
      { "3", "Async (Skip Rendering)" }, // ShaderCompilationMode::AsynchronousSkipRendering
      { "1", "Sync (UberShaders)" }, // ShaderCompilationMode::SynchronousUberShaders
      { "2", "Async (UberShaders)" }, // ShaderCompilationMode::AsynchronousUberShaders
      { nullptr, nullptr }
    },
    STR(ShaderCompilationMode::Synchronous)
  },
  {
    Libretro::Options::gfx_settings::WAIT_FOR_SHADERS,
    "Graphics > Settings > Wait for Shaders",
    "Wait for Shaders",
    "Precompile shaders before starting.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_settings::ANTI_ALIASING,
    "Graphics > Settings > Anti-Aliasing",
    "Anti-Aliasing",
    "Reduce jagged edges.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "0", "None" },
      { "1", "2x MSAA" },
      { "2", "4x MSAA" },
      { "3", "8x MSAA" },
      { "4", "2x SSAA" },
      { "5", "4x SSAA" },
      { "6", "8x SSAA" },
      { nullptr, nullptr }
    },
    "0"
  },
  {
    Libretro::Options::gfx_settings::TEXTURE_CACHE_ACCURACY,
    "Graphics > Settings > Texture Cache Accuracy",
    "Texture Cache Accuracy",
    "Texture cache safety level.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "128", "Fast" },
      { "512", "Middle" },
      { "0",   "Safe" },
      { nullptr, nullptr }
    },
    "128"
  },
  {
    Libretro::Options::gfx_settings::GPU_TEXTURE_DECODING,
    "Graphics > Settings > GPU Texture Decoding",
    "GPU Texture Decoding",
    "Decode textures on GPU.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_settings::ENABLE_PIXEL_LIGHTING,
    "Graphics > Settings > Pixel Lighting",
    "Pixel Lighting",
    "Enable per-pixel lighting calculations instead of per-vertex.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_settings::FAST_DEPTH_CALCULATION,
    "Graphics > Settings > Fast Depth Calculation",
    "Fast Depth Calculation",
    "Use faster depth calculation.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_settings::DISABLE_FOG,
    "Graphics > Settings > Disable Fog",
    "Disable Fog",
    "Disable fog rendering effects. May improve performance but reduces visual accuracy.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },

  // ========== Graphics.Enhancements ==========
  {
    Libretro::Options::gfx_enhancements::FORCE_TEXTURE_FILTERING_MODE,
    "Graphics > Enhancements > Texture Filtering",
    "Texture Filtering",
    "Override texture filtering.",
    nullptr,
    CATEGORY_GFX_ENHANCEMENTS,
    {
      { "0", "Default" }, // TextureFilteringMode::Default
      { "1", "Nearest (Sharp)" }, // TextureFilteringMode::Nearest
      { "2",  "Linear (Smooth)" }, // TextureFilteringMode::Linear
      { nullptr, nullptr }
    },
    "0"
  },
  {
    Libretro::Options::gfx_enhancements::MAX_ANISOTROPY,
    "Graphics > Enhancements > Anisotropic Filtering",
    "Anisotropic Filtering",
    "Improve texture quality at angles.",
    nullptr,
    CATEGORY_GFX_ENHANCEMENTS,
    {
      { "0", "1x (Off)" }, // AnisotropicFilteringMode::Force1x
      { "1", "2x" }, // AnisotropicFilteringMode::Force2x
      { "2", "4x" }, // AnisotropicFilteringMode::Force4x
      { "3", "8x" }, // AnisotropicFilteringMode::Force8x
      { "4", "16x" }, // AnisotropicFilteringMode::Force16x
      { nullptr, nullptr }
    },
    "0"
  },
  {
    Libretro::Options::gfx_enhancements::LOAD_CUSTOM_TEXTURES,
    "Graphics > Enhancements > Load Custom Textures",
    "Load Custom Textures",
    "Load high-res texture packs.",
    nullptr,
    CATEGORY_GFX_ENHANCEMENTS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_enhancements::CACHE_CUSTOM_TEXTURES,
    "Graphics > Enhancements > Prefetch Custom Textures",
    "Prefetch Custom Textures",
    "Preload custom textures.",
    nullptr,
    CATEGORY_GFX_ENHANCEMENTS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_enhancements::GFX_ENHANCE_OUTPUT_RESAMPLING,
    "Graphics > Enhancements > Output Resampling",
    "Output Resampling",
    "Select the resampling filter used when scaling the final image.",
    nullptr,
    CATEGORY_GFX_ENHANCEMENTS,
    {
      { "0", "Default" },
      { "1", "Bilinear" },
      { "2", "B-Spline" },
      { "3","Mitchell-Netravali" },
      { "4", "Catmull-Rom" },
      { "5", "Sharp Bilinear" },
      { "6", "Area Sampling" },
      { nullptr, nullptr }
    },
    "default"
  },
  {
    Libretro::Options::gfx_enhancements::FORCE_TRUE_COLOR,
    "Graphics > Enhancements > Force True Color",
    "Force True Color",
    "Disable dithering and force 24-bit color output instead of 18-bit.",
    nullptr,
    CATEGORY_GFX_ENHANCEMENTS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_enhancements::GFX_ENHANCE_DISABLE_COPY_FILTER,
    "Graphics > Enhancements > Disable Copy Filter",
    "Disable Copy Filter",
    "Disable the GameCube/Wii copy filter. Removes blur from some games but may reduce accuracy.",
    nullptr,
    CATEGORY_GFX_ENHANCEMENTS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_enhancements::GFX_ENHANCE_HDR_OUTPUT,
    "Graphics > Enhancements > HDR Output",
    "HDR Output",
    "Enable High Dynamic Range output when supported by the graphics backend and display.",
    nullptr,
    CATEGORY_GFX_ENHANCEMENTS,
    {
      { "disabled", "Disabled" },
      { "enabled",  "Enabled" },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_enhancements::GFX_ARBITRARY_MIPMAP_DETECTION,
    "Graphics > Enhancements > Arbitrary Mipmap Detection",
    "Arbitrary Mipmap Detection",
    "Enable detection of arbitrary mipmaps. Improves accuracy in some games but may reduce performance.",
    nullptr,
    CATEGORY_GFX_ENHANCEMENTS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },

  // ========== Graphics.Hacks ==========
  {
    Libretro::Options::gfx_hacks::EFB_ACCESS_ENABLE,
    "Graphics > Hacks > EFB Access from CPU",
    "EFB Access from CPU",
    "Allow CPU EFB access. Required for some games but slow.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_hacks::EFB_ACCESS_DEFER_INVALIDATION,
    "Graphics > Hacks > EFB Access Defer Invalidation",
    "EFB Access Defer Invalidation",
    "Defer EFB cache invalidation.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_hacks::EFB_ACCESS_TILE_SIZE,
    "Graphics > Hacks > EFB Access Tile Size",
    "EFB Access Tile Size",
    "EFB access granularity.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "1",   "1 (per-pixel, slowest/most accurate)" },
      { "4",   "4" },
      { "8",   "8" },
      { "16",  "16" },
      { "32",  "32" },
      { "64",  "64 (default)" },
      { nullptr, nullptr }
    },
    "64"
  },
  {
    Libretro::Options::gfx_hacks::BBOX_ENABLED,
    "Graphics > Hacks > Bounding Box",
    "Bounding Box",
    "Emulate bounding box hardware. Required for Paper Mario TTYD.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_hacks::FORCE_PROGRESSIVE,
    "Graphics > Hacks > Force Progressive",
    "Force Progressive",
    "Force progressive scan.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_hacks::EFB_TO_TEXTURE,
    "Graphics > Hacks > Skip EFB Copy to RAM",
    "Skip EFB Copy to RAM",
    "Store EFB in texture memory.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_hacks::XFB_TO_TEXTURE_ENABLE,
    "Graphics > Hacks > Skip XFB Copy to RAM",
    "Skip XFB Copy to RAM",
    "Store XFB in texture memory.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_hacks::EFB_TO_VRAM,
    "Graphics > Hacks > Disable EFB to VRAM",
    "Disable EFB to VRAM",
    "Disable EFB VRAM copies.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_hacks::DEFER_EFB_COPIES,
    "Graphics > Hacks > Defer EFB Copies",
    "Defer EFB Copies",
    "Defer EFB copies until needed.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_hacks::IMMEDIATE_XFB,
    "Graphics > Hacks > Immediate XFB",
    "Immediate XFB",
    "Display XFB immediately.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_hacks::SKIP_DUPE_FRAMES,
    "Graphics > Hacks > Skip Duplicate Frames",
    "Skip Duplicate Frames",
    "Don't present duplicate frames.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_hacks::EARLY_XFB_OUTPUT,
    "Graphics > Hacks > Early XFB Output",
    "Early XFB Output",
    "Output XFB early.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_hacks::EFB_SCALED_COPY,
    "Graphics > Hacks > EFB Scaled Copy",
    "EFB Scaled Copy",
    "Scale EFB copy by IR.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  {
    Libretro::Options::gfx_hacks::EFB_EMULATE_FORMAT_CHANGES,
    "Graphics > Hacks > EFB Emulate Format Changes",
    "EFB Emulate Format Changes",
    "Emulate EFB format changes (needed for some effects).",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_hacks::VERTEX_ROUNDING,
    "Graphics > Hacks > Vertex Rounding",
    "Vertex Rounding",
    "Round vertex positions to avoid gaps.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_hacks::VI_SKIP,
    "Graphics > Hacks > VI Skip",
    "VI Skip",
    "Skip VI updates to improve performance.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  {
    Libretro::Options::gfx_hacks::FAST_TEXTURE_SAMPLING,
    "Graphics > Hacks > Fast Texture Sampling",
    "Fast Texture Sampling",
    "Use faster but less accurate texture sampling.",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "enabled"
  },
  #ifdef __APPLE__
  {
    Libretro::Options::gfx_hacks::NO_MIPMAPPING,
    "Graphics > Hacks > Disable Mipmapping",
    "Disable Mipmapping",
    "Disable mipmapping (workaround for macOS drivers).",
    nullptr,
    CATEGORY_GFX_HACKS,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
  },
  #endif

  // ========== Wiimote IR ==========
  {
    Libretro::Options::wiimote::IR_MODE,
    "Wiimote IR > Wiimote IR Mode",
    "Wiimote IR Mode",
    "Control method for Wiimote pointer.",
    nullptr,
    CATEGORY_WIIMOTE,
    {
      { "0", "Right Stick controls pointer (relative)" },
      { "1", "Right Stick controls pointer (absolute)" },
      { "2", "Mouse controls pointer" },
      { nullptr, nullptr }
    },
    "1"
  },
  {
    Libretro::Options::wiimote::IR_OFFSET,
    "Wiimote IR > Wiimote IR Vertical Offset",
    "Wiimote IR Vertical Offset",
    "Adjust vertical center of Wiimote pointer.",
    nullptr,
    CATEGORY_WIIMOTE,
    {
      // Positive offsets
      { "10", nullptr }, { "11", nullptr }, { "12", nullptr }, { "13", nullptr }, { "14", nullptr },
      { "15", nullptr }, { "16", nullptr }, { "17", nullptr }, { "18", nullptr }, { "19", nullptr },
      { "20", nullptr }, { "21", nullptr }, { "22", nullptr }, { "23", nullptr }, { "24", nullptr },
      { "25", nullptr }, { "26", nullptr }, { "27", nullptr }, { "28", nullptr }, { "29", nullptr },
      { "30", nullptr }, { "31", nullptr }, { "32", nullptr }, { "33", nullptr }, { "34", nullptr },
      { "35", nullptr }, { "36", nullptr }, { "37", nullptr }, { "38", nullptr }, { "39", nullptr },
      { "40", nullptr }, { "41", nullptr }, { "42", nullptr }, { "43", nullptr }, { "44", nullptr },
      { "45", nullptr }, { "46", nullptr }, { "47", nullptr }, { "48", nullptr }, { "49", nullptr },
      { "50", nullptr },
      // Negative offsets
      { "-50", nullptr }, { "-49", nullptr }, { "-48", nullptr }, { "-47", nullptr }, { "-46", nullptr },
      { "-45", nullptr }, { "-44", nullptr }, { "-43", nullptr }, { "-42", nullptr }, { "-41", nullptr },
      { "-40", nullptr }, { "-39", nullptr }, { "-38", nullptr }, { "-37", nullptr }, { "-36", nullptr },
      { "-35", nullptr }, { "-34", nullptr }, { "-33", nullptr }, { "-32", nullptr }, { "-31", nullptr },
      { "-30", nullptr }, { "-29", nullptr }, { "-28", nullptr }, { "-27", nullptr }, { "-26", nullptr },
      { "-25", nullptr }, { "-24", nullptr }, { "-23", nullptr }, { "-22", nullptr }, { "-21", nullptr },
      { "-20", nullptr }, { "-19", nullptr }, { "-18", nullptr }, { "-17", nullptr }, { "-16", nullptr },
      { "-15", nullptr }, { "-14", nullptr }, { "-13", nullptr }, { "-12", nullptr }, { "-11", nullptr },
      { "-10", nullptr }, { "-9", nullptr }, { "-8", nullptr }, { "-7", nullptr }, { "-6", nullptr },
      { "-5", nullptr }, { "-4", nullptr }, { "-3", nullptr }, { "-2", nullptr }, { "-1", nullptr },
      // Zero
      { "0", "0 (Center)" },
      // Small positives
      { "1", nullptr }, { "2", nullptr }, { "3", nullptr }, { "4", nullptr }, { "5", nullptr },
      { "6", nullptr }, { "7", nullptr }, { "8", nullptr }, { "9", nullptr },
      { nullptr, nullptr }
    },
    "0"
  },
  {
    Libretro::Options::wiimote::IR_YAW,
    "Wiimote IR > Wiimote IR Total Yaw",
    "Wiimote IR Total Yaw",
    "Horizontal field of view for Wiimote pointer.",
    nullptr,
    CATEGORY_WIIMOTE,
    {
      { "15", nullptr }, { "16", nullptr }, { "17", nullptr }, { "18", nullptr }, { "19", nullptr },
      { "20", nullptr }, { "21", nullptr }, { "22", nullptr }, { "23", nullptr }, { "24", nullptr },
      { "25", "25 (Default)" }, { "26", nullptr }, { "27", nullptr }, { "28", nullptr }, { "29", nullptr },
      { "30", nullptr }, { "31", nullptr }, { "32", nullptr }, { "33", nullptr }, { "34", nullptr },
      { "35", nullptr }, { "36", nullptr }, { "37", nullptr }, { "38", nullptr }, { "39", nullptr },
      { "40", nullptr }, { "41", nullptr }, { "42", nullptr }, { "43", nullptr }, { "44", nullptr },
      { "45", nullptr }, { "46", nullptr }, { "47", nullptr }, { "48", nullptr }, { "49", nullptr },
      { "50", nullptr }, { "51", nullptr }, { "52", nullptr }, { "53", nullptr }, { "54", nullptr },
      { "55", nullptr }, { "56", nullptr }, { "57", nullptr }, { "58", nullptr }, { "59", nullptr },
      { "60", nullptr }, { "61", nullptr }, { "62", nullptr }, { "63", nullptr }, { "64", nullptr },
      { "65", nullptr }, { "66", nullptr }, { "67", nullptr }, { "68", nullptr }, { "69", nullptr },
      { "70", nullptr }, { "71", nullptr }, { "72", nullptr }, { "73", nullptr }, { "74", nullptr },
      { "75", nullptr }, { "76", nullptr }, { "77", nullptr }, { "78", nullptr }, { "79", nullptr },
      { "80", nullptr }, { "81", nullptr }, { "82", nullptr }, { "83", nullptr }, { "84", nullptr },
      { "85", nullptr }, { "86", nullptr }, { "87", nullptr }, { "88", nullptr }, { "89", nullptr },
      { "90", nullptr }, { "91", nullptr }, { "92", nullptr }, { "93", nullptr }, { "94", nullptr },
      { "95", nullptr }, { "96", nullptr }, { "97", nullptr }, { "98", nullptr }, { "99", nullptr },
      { "100", nullptr },
      { "0", nullptr }, { "1", nullptr }, { "2", nullptr }, { "3", nullptr }, { "4", nullptr },
      { "5", nullptr }, { "6", nullptr }, { "7", nullptr }, { "8", nullptr }, { "9", nullptr },
      { "10", nullptr }, { "11", nullptr }, { "12", nullptr }, { "13", nullptr }, { "14", nullptr },
      { nullptr, nullptr }
    },
    "25"
  },
  {
    Libretro::Options::wiimote::IR_PITCH,
    "Wiimote IR > Wiimote IR Total Pitch",
    "Wiimote IR Total Pitch",
    "Vertical field of view for Wiimote pointer.",
    nullptr,
    CATEGORY_WIIMOTE,
    {
      { "15", nullptr },
      { "16", nullptr }, { "17", nullptr }, { "18", nullptr }, { "19", nullptr },
      { "20", nullptr }, { "21", nullptr }, { "22", nullptr }, { "23", nullptr }, { "24", nullptr },
      { "25", "25 (Default)" }, { "26", nullptr }, { "27", nullptr }, { "28", nullptr }, { "29", nullptr },
      { "30", nullptr }, { "31", nullptr }, { "32", nullptr }, { "33", nullptr }, { "34", nullptr },
      { "35", nullptr }, { "36", nullptr }, { "37", nullptr }, { "38", nullptr }, { "39", nullptr },
      { "40", nullptr }, { "41", nullptr }, { "42", nullptr }, { "43", nullptr }, { "44", nullptr },
      { "45", nullptr }, { "46", nullptr }, { "47", nullptr }, { "48", nullptr }, { "49", nullptr },
      { "50", nullptr }, { "51", nullptr }, { "52", nullptr }, { "53", nullptr }, { "54", nullptr },
      { "55", nullptr }, { "56", nullptr }, { "57", nullptr }, { "58", nullptr }, { "59", nullptr },
      { "60", nullptr }, { "61", nullptr }, { "62", nullptr }, { "63", nullptr }, { "64", nullptr },
      { "65", nullptr }, { "66", nullptr }, { "67", nullptr }, { "68", nullptr }, { "69", nullptr },
      { "70", nullptr }, { "71", nullptr }, { "72", nullptr }, { "73", nullptr }, { "74", nullptr },
      { "75", nullptr }, { "76", nullptr }, { "77", nullptr }, { "78", nullptr }, { "79", nullptr },
      { "80", nullptr }, { "81", nullptr }, { "82", nullptr }, { "83", nullptr }, { "84", nullptr },
      { "85", nullptr }, { "86", nullptr }, { "87", nullptr }, { "88", nullptr }, { "89", nullptr },
      { "90", nullptr }, { "91", nullptr }, { "92", nullptr }, { "93", nullptr }, { "94", nullptr },
      { "95", nullptr }, { "96", nullptr }, { "97", nullptr }, { "98", nullptr }, { "99", nullptr },
      { "100", nullptr },
      { "0", nullptr }, { "1", nullptr }, { "2", nullptr }, { "3", nullptr }, { "4", nullptr },
      { "5", nullptr }, { "6", nullptr }, { "7", nullptr }, { "8", nullptr }, { "9", nullptr },
      { "10", nullptr }, { "11", nullptr }, { "12", nullptr }, { "13", nullptr }, { "14", nullptr },
      { nullptr, nullptr }
    },
    "25"
  },

  { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{0}}, nullptr }
};

void Init()
{
  SetVariables();
  RegisterCache();
  CheckForUpdatedVariables();
}

void SetVariables()
{
  unsigned version = 0;

  struct retro_core_options_v2 options_us = {
    const_cast<retro_core_option_v2_category*>(option_cats),
    const_cast<retro_core_option_v2_definition*>(option_defs)
  };

  if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
    version = 0;

  if (version >= 2)
  {
    environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_us);
  }
  else
  {
    size_t i, j;
    size_t option_index              = 0;
    size_t num_options               = 0;
    struct retro_core_option_definition
        *option_v1_defs_us           = NULL;
    struct retro_variable *variables = NULL;
    char **values_buf                = NULL;

    /* Determine total number of options */
    while (true)
    {
      if (option_defs[num_options].key)
        num_options++;
      else
        break;
    }

    if (version >= 1)
    {
      /* Allocate US array */
      option_v1_defs_us = (struct retro_core_option_definition *)
          calloc(num_options + 1, sizeof(struct retro_core_option_definition));

      /* Copy parameters from option_defs array */
      for (i = 0; i < num_options; i++)
      {
        struct retro_core_option_v2_definition *option_def_us = &option_defs[i];
        struct retro_core_option_value *option_values         = option_def_us->values;
        struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];
        struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;

        option_v1_def_us->key           = option_def_us->key;
        option_v1_def_us->desc          = option_def_us->desc;
        option_v1_def_us->info          = option_def_us->info;
        option_v1_def_us->default_value = option_def_us->default_value;

        /* Values must be copied individually... */
        while (option_values->value)
        {
          option_v1_values->value = option_values->value;
          option_v1_values->label = option_values->label;

          option_values++;
          option_v1_values++;
        }
      }

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);
    }
    else
    {
      /* Allocate arrays */
      variables  = (struct retro_variable *)calloc(num_options + 1,
          sizeof(struct retro_variable));
      values_buf = (char **)calloc(num_options, sizeof(char *));

      if (!variables || !values_buf)
        goto error;

      /* Copy parameters from option_defs array */
      for (i = 0; i < num_options; i++)
      {
        const char *key                        = option_defs[i].key;
        const char *desc                       = option_defs[i].desc;
        const char *default_value              = option_defs[i].default_value;
        struct retro_core_option_value *values = option_defs[i].values;
        size_t buf_len                         = 3;
        size_t default_index                   = 0;

        values_buf[i] = NULL;

        if (desc)
        {
          size_t num_values = 0;

          /* Determine number of values */
          while (true)
          {
            if (values[num_values].value)
            {
              /* Check if this is the default value */
              if (default_value)
                if (strcmp(values[num_values].value, default_value) == 0)
                  default_index = num_values;

              buf_len += strlen(values[num_values].value);
              num_values++;
            }
            else
              break;
          }

          /* Build values string */
          if (num_values > 0)
          {
            buf_len += num_values - 1;
            buf_len += strlen(desc);

            values_buf[i] = (char *)calloc(buf_len, sizeof(char));
            if (!values_buf[i])
              goto error;

            strcpy(values_buf[i], desc);
            strcat(values_buf[i], "; ");

            /* Default value goes first */
            strcat(values_buf[i], values[default_index].value);

            /* Add remaining values */
            for (j = 0; j < num_values; j++)
            {
              if (j != default_index)
              {
                strcat(values_buf[i], "|");
                strcat(values_buf[i], values[j].value);
              }
            }
          }
        }

        variables[option_index].key   = key;
        variables[option_index].value = values_buf[i];
        option_index++;
      }

      /* Set variables */
      environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
    }

error:
    /* Clean up */

    if (option_v1_defs_us)
    {
      free(option_v1_defs_us);
      option_v1_defs_us = NULL;
    }

    if (values_buf)
    {
      for (i = 0; i < num_options; i++)
      {
        if (values_buf[i])
        {
          free(values_buf[i]);
          values_buf[i] = NULL;
        }
      }

      free(values_buf);
      values_buf = NULL;
    }

    if (variables)
    {
      free(variables);
      variables = NULL;
    }
  }
}

void RegisterCache()
{
  for (const retro_core_option_v2_definition* def = option_defs;
       def && def->key;
       ++def)
  {
    std::string val = def->default_value ? def->default_value : "";
    retro_variable var{ def->key, nullptr };
    if (::Libretro::environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      val = var.value;
    optionCache[def->key] = val;
  }
}

void CheckForUpdatedVariables()
{
  bool updated = false;
  if (::Libretro::environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && !updated)
    return;

  for (auto& [key, oldVal] : optionCache)
  {
    retro_variable var{ key.c_str(), nullptr };
    if (::Libretro::environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
      std::string newVal = var.value;
      if (newVal != oldVal)
      {
        oldVal = newVal;
        optionDirty[key] = true;
      }
    }
  }
}

bool IsUpdated(const char* key)
{
  auto it = optionDirty.find(key);
  if (it != optionDirty.end() && it->second) {
    it->second = false; // consume the dirty flag
    return true;
  }

  return false;
}

// bool specialisation
template <>
bool GetCached<bool>(const char* key, const bool def)
{
  auto it = optionCache.find(key);
  if (it == optionCache.end())
    return def;
  const std::string& v = it->second;
  if (v == "enabled" || v == "true" || v == "1") return true;
  if (v == "disabled" || v == "false" || v == "0") return false;
  return def;
}

// int specialisation
template <>
int GetCached<int>(const char* key, const int def)
{
  auto it = optionCache.find(key);
  if (it == optionCache.end())
    return def;
  char* end = nullptr;
  long parsed = strtol(it->second.c_str(), &end, 10);
  return (end != it->second.c_str()) ? static_cast<int>(parsed) : def;
}

// double specialisation
template <>
double GetCached<double>(const char* key, double def)
{
  auto it = optionCache.find(key);
  if (it == optionCache.end())
    return def;
  char* end = nullptr;
  double parsed = strtod(it->second.c_str(), &end);
  return (end != it->second.c_str()) ? parsed : def;
}

// std::string specialisation
template <>
std::string GetCached<std::string>(const char* key, const std::string def)
{
  auto it = optionCache.find(key);
  return (it != optionCache.end()) ? it->second : def;
}
}  // namespace Options

inline bool GetVariable(const char* key, const char*& out_value)
{
  retro_variable var{ key, nullptr };
  if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
  {
    out_value = var.value;
    return true;
  }
  return false;
}

template <>
bool GetOption<bool>(const char* key, bool def)
{
  const char* val = nullptr;
  if (GetVariable(key, val))
  {
    if (!strcmp(val, "enabled") || !strcmp(val, "true") || !strcmp(val, "1"))
      return true;
    if (!strcmp(val, "disabled") || !strcmp(val, "false") || !strcmp(val, "0"))
      return false;
  }
  return def;
}

template <>
int GetOption<int>(const char* key, int def)
{
  const char* val = nullptr;
  if (GetVariable(key, val))
  {
    char* end = nullptr;
    long parsed = strtol(val, &end, 10);
    if (end != val)
      return static_cast<int>(parsed);
  }
  return def;
}

template <>
double GetOption<double>(const char* key, double def)
{
  const char* val = nullptr;
  if (GetVariable(key, val))
  {
    char* end = nullptr;
    double parsed = strtod(val, &end);
    if (end != val)
      return parsed;
  }
  return def;
}

template <>
std::string GetOption<std::string>(const char* key, std::string def)
{
  const char* val = nullptr;
  if (GetVariable(key, val))
    return std::string(val);
  return def;
}
}  // namespace Libretro
