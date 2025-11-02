
#include <libretro.h>
#include <cstdlib>
#include "DolphinLibretro/Common/Options.h"

namespace Libretro
{
namespace Options
{
static std::vector<retro_variable> optionsList;
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
    "CPU Core",
    nullptr,
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
    "CPU Clock Rate",
    nullptr,
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
    "Emulation Speed",
    nullptr,
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
  /*{
    Libretro::Options::core::MAIN_CPU_THREAD,
    "Dual Core Mode",
    nullptr,
    "Enable dual-core CPU emulation.",
    nullptr,
    CATEGORY_CORE,
    {
      { "disabled", "Disabled" },
      { "enabled",  "Enabled" },
      { nullptr, nullptr }
    },
    "enabled"
  },*/
  {
    Libretro::Options::core::MAIN_PRECISION_FRAME_TIMING,
    "Precision Frame Timing",
    nullptr,
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
    "Fastmem",
    nullptr,
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
    "Fastmem Arena",
    nullptr,
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
    Libretro::Options::audio::DSP_HLE,
    "DSP HLE",
    nullptr,
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
    Libretro::Options::core::CHEATS_ENABLED,
    "Internal Cheats",
    nullptr,
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
    Libretro::Options::core::LANGUAGE,
    "System Language",
    nullptr,
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
    "Speed Up Disc Transfer",
    nullptr,
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
    "Enable MMU",
    nullptr,
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
    Libretro::Options::sysconf::WIIMOTE_CONTINUOUS_SCANNING,
    "Wiimote Continuous Scanning",
    nullptr,
    "Continuously scan for Wiimotes.",
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
    Libretro::Options::sysconf::ALT_GC_PORTS_ON_WII,
    "Alt GC Ports (Wii)",
    nullptr,
    "Use ports 5-8 for GameCube controllers in Wii mode.",
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
    Libretro::Options::audio::MIXER_RATE,
    "Audio Mixer Rate",
    nullptr,
    "Audio sample rate.",
    nullptr,
    CATEGORY_AUDIO,
    {
      { "32000", "32000 Hz" },
      { "48000", "48000 Hz" },
      { nullptr, nullptr }
    },
    "32000"
  },

  // ========== Main.Interface ==========
  {
    Libretro::Options::main_interface::OSD_ENABLED,
    "On-Screen Display",
    nullptr,
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
    Libretro::Options::main_interface::ENABLE_DEBUGGING,
    "Enable debugging",
    nullptr,
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
    Libretro::Options::audio::DSP_JIT,
    "DSP JIT",
    nullptr,
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

  // ========== SYSCONF.IPL ==========
  {
    Libretro::Options::sysconf::WIDESCREEN,
    "Widescreen (Wii)",
    nullptr,
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
    "Progressive Scan",
    nullptr,
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
    "PAL60 Mode",
    nullptr,
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
    "Sensor Bar Position",
    nullptr,
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
    "Controller Rumble",
    nullptr,
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

  // ========== Graphics.Hardware ==========
  /*{
    Libretro::Options::gfx_hardware::VSYNC,
    "VSync",
    nullptr,
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
    "Graphics Backend",
    nullptr,
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
    "Widescreen Hack",
    nullptr,
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
    Libretro::Options::gfx_settings::EFB_SCALE,
    "Internal Resolution",
    nullptr,
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
    "Shader Compilation",
    nullptr,
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
    "Wait for Shaders",
    nullptr,
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
    "Anti-Aliasing",
    nullptr,
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
    "Texture Cache Accuracy",
    nullptr,
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
    Libretro::Options::gfx_enhancements::LOAD_CUSTOM_TEXTURES,
    "Load Custom Textures",
    nullptr,
    "Load high-res texture packs.",
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
    Libretro::Options::gfx_enhancements::CACHE_CUSTOM_TEXTURES,
    "Prefetch Custom Textures",
    nullptr,
    "Preload custom textures.",
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
    Libretro::Options::gfx_settings::GPU_TEXTURE_DECODING,
    "GPU Texture Decoding",
    nullptr,
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
    "Pixel Lighting",
    nullptr,
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
    "Fast Depth Calculation",
    nullptr,
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
    "Disable Fog",
    nullptr,
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
    "Texture Filtering",
    nullptr,
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
    "Anisotropic Filtering",
    nullptr,
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
    Libretro::Options::gfx_enhancements::GFX_ENHANCE_OUTPUT_RESAMPLING,
    "Output Resampling",
    nullptr,
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
    "Force True Color",
    nullptr,
    "Disable dithering and force 24-bit color output instead of 18-bit.",
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
    Libretro::Options::gfx_enhancements::GFX_ENHANCE_DISABLE_COPY_FILTER,
    "Disable Copy Filter",
    nullptr,
    "Disable the GameCube/Wii copy filter. Removes blur from some games but may reduce accuracy.",
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
    Libretro::Options::gfx_enhancements::GFX_ENHANCE_HDR_OUTPUT,
    "HDR Output",
    nullptr,
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
    "Arbitrary Mipmap Detection",
    nullptr,
    "Enable detection of arbitrary mipmaps. Improves accuracy in some games but may reduce performance.",
    nullptr,
    CATEGORY_GFX_SETTINGS,
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
    "EFB Access from CPU",
    nullptr,
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
    "EFB Access Defer Invalidation",
    nullptr,
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
    "EFB Access Tile Size",
    nullptr,
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
    "Bounding Box",
    nullptr,
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
    "Force Progressive",
    nullptr,
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
    "Skip EFB Copy to RAM",
    nullptr,
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
    "Skip XFB Copy to RAM",
    nullptr,
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
    "Disable EFB to VRAM",
    nullptr,
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
    "Defer EFB Copies",
    nullptr,
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
    "Immediate XFB",
    nullptr,
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
    "Skip Duplicate Frames",
    nullptr,
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
    "Early XFB Output",
    nullptr,
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
    "EFB Scaled Copy",
    nullptr,
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
    "EFB Emulate Format Changes",
    nullptr,
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
    "Vertex Rounding",
    nullptr,
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
    "VI Skip",
    nullptr,
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
    "Fast Texture Sampling",
    nullptr,
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
    "Disable Mipmapping",
    nullptr,
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
    "Wiimote IR Mode",
    nullptr,
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
    "Wiimote IR Vertical Offset",
    nullptr,
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
    "Wiimote IR Total Yaw",
    nullptr,
    "Horizontal field of view for Wiimote pointer.",
    nullptr,
    CATEGORY_WIIMOTE,
    {
      { "15", "15 (Default)" }, { "16", nullptr }, { "17", nullptr }, { "18", nullptr }, { "19", nullptr },
      { "20", nullptr }, { "21", nullptr }, { "22", nullptr }, { "23", nullptr }, { "24", nullptr },
      { "25", nullptr }, { "26", nullptr }, { "27", nullptr }, { "28", nullptr }, { "29", nullptr },
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
    "15"
  },
  {
    Libretro::Options::wiimote::IR_PITCH,
    "Wiimote IR Total Pitch",
    nullptr,
    "Vertical field of view for Wiimote pointer.",
    nullptr,
    CATEGORY_WIIMOTE,
    {
      { "15", "15 (Default)" },
      { "16", nullptr }, { "17", nullptr }, { "18", nullptr }, { "19", nullptr },
      { "20", nullptr }, { "21", nullptr }, { "22", nullptr }, { "23", nullptr }, { "24", nullptr },
      { "25", nullptr }, { "26", nullptr }, { "27", nullptr }, { "28", nullptr }, { "29", nullptr },
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
    "15"
  },

  // ========== Other ==========
  {
    Libretro::Options::main_interface::LOG_LEVEL,
    "Log Level",
    nullptr,
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
    Libretro::Options::audio::CALL_BACK_AUDIO,
    "Async Audio Callback",
    nullptr,
    "Use asynchronous audio callbacks.",
    "Pushes audio asynchronously instead of synchronously.",
    CATEGORY_AUDIO,
    {
      { "disabled", nullptr },
      { "enabled",  nullptr },
      { nullptr, nullptr }
    },
    "disabled"
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

  if (::Libretro::environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) &&
      version >= 2)
  {
    struct retro_core_options_v2 options_v2 = {
      const_cast<retro_core_option_v2_category*>(option_cats),
      const_cast<retro_core_option_v2_definition*>(option_defs)
    };

    if (::Libretro::environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_v2))
      return;
  }

  // fall back
  optionsList.clear();

  for (const retro_core_option_v2_definition* def = option_defs;
    def && def->key;
    ++def)
  {
    optionsList.push_back({def->key, def->desc});
  }

  if (!optionsList.empty() && optionsList.back().key)
    optionsList.push_back({});
  ::Libretro::environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)optionsList.data());
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
