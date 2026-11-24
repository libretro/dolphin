
add_library(dolphin_libretro SHARED
  Audio.cpp
  Boot.cpp
  Host.cpp
  Input.cpp
  Log.cpp
  Main.cpp
  Video.cpp
  VR.cpp
)

if(ENABLE_VULKAN)
  target_sources(dolphin_libretro PRIVATE
    Vulkan.cpp
  )
endif()

include_directories(${CMAKE_SOURCE_DIR}/Externals/libretro-common/include)

if(ENABLE_VULKAN)
	include_directories(${CMAKE_SOURCE_DIR}/Externals/Vulkan-Headers/include)
	include_directories(${CMAKE_SOURCE_DIR}/Externals/VulkanMemoryAllocator/include)
endif()

target_include_directories(dolphin_libretro PRIVATE ${CMAKE_SOURCE_DIR}/Externals/libretro-common/include)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set_target_properties(dolphin_libretro PROPERTIES PREFIX "")
if(ANDROID)
   target_sources(dolphin_libretro PRIVATE
     ../../Android/jni/AndroidCommon/IDCache.cpp
   )
   set_target_properties(dolphin_libretro PROPERTIES SUFFIX "_android.so")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
   set(CLANG 1)
endif()

if(NOT MSVC AND NOT CLANG AND NOT SWITCH2)
   set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif()

if(MSVC)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DELAYLOAD:OpenGL32.dll /DELAYLOAD:DInput8.dll")
	target_link_libraries(dolphin_libretro PRIVATE
		Delayimp.lib
		core
		dxguid
		hid
		uicommon
		windowsapp
		${LIBS}
	)
else()
	target_link_libraries(dolphin_libretro PRIVATE
		core
		uicommon
		${LIBS}
	 )
endif()

set(LIBRETRO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Externals/libretro-common/include)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Common)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/VideoContexts)

target_link_libraries(dolphin_libretro PRIVATE
  dolphin_libretro_common
  dolphin_libretro_videocontexts
)

if(MINGW)
  target_link_libraries(dolphin_libretro PRIVATE
    common
    setupapi
  )
endif()

if(MINGW OR LIBRETRO_STATIC OR WEBOS)
	target_link_options(dolphin_libretro PRIVATE -static-libgcc -static-libstdc++)
endif()

if(MINGW AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_link_options(dolphin_libretro PRIVATE -Wl,--allow-multiple-definition)
endif()

if(APPLE)
  target_sources(dolphin_libretro PRIVATE
    ${CMAKE_SOURCE_DIR}/Source/Core/Common/CommonFuncsObjC.mm
  )
endif()

if(SWITCH2)
  set_target_properties(dolphin_libretro PROPERTIES
    OUTPUT_NAME "dolphin_libretro"
    SUFFIX ".so"
    NO_SONAME TRUE
  )

  target_link_options(dolphin_libretro PRIVATE
    "-Wl,-soname,dolphin_libretro_libounce.nro"
    "-Wl,--version-script=${CMAKE_SOURCE_DIR}/Source/Core/DolphinLibretro/link.T"
  )

  add_custom_command(TARGET dolphin_libretro POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${CMAKE_CURRENT_BINARY_DIR}/nro"

    COMMAND "${NX_ELF2NRO_EXE}"
      "$<TARGET_FILE:dolphin_libretro>"
      "${CMAKE_CURRENT_BINARY_DIR}/nro/dolphin_libretro_libounce.nro"

    COMMAND "${CMAKE_COMMAND}" -E copy
      "${CMAKE_CURRENT_BINARY_DIR}/nro/dolphin_libretro_libounce.nro"
      "${PROJECT_BINARY_DIR}/dolphin_libretro_libounce.nro"

    VERBATIM
  )
endif()
