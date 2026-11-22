// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <string>

#include "Common/CommonTypes.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace Common
{
void* AllocateExecutableMemory(size_t size);
#if defined(__LIBRETRO__) && defined(__SWITCH2__)
void PublishExecutableMemory(void* start, size_t size);
#endif

#ifdef IPHONEOS
// Which of the executable-memory strategies is in use. Dual-mapped modes write
// through an alias, so the W^X toggling below must not run for them.
enum class JitType
{
  Legacy,
  LuckNoTXM,
  LuckTXM
};

void SetJitType(JitType type);
#endif

#if defined(__SWITCH2__) || defined(IPHONEOS) || (defined(__APPLE__) && defined(__aarch64__) && !TARGET_OS_IPHONE)
ptrdiff_t AllocateWritableRegionAndGetDiff(void* rx_ptr, size_t size);
void FreeWritableRegion(void* rx_ptr, size_t size, ptrdiff_t diff);
void FreeExecutableMemory(void* ptr, size_t size);
#endif

// These two functions control the executable/writable state of the W^X memory
// allocations. More detailed documentation about them is in the .cpp file.
// In general where applicable the ScopedJITPageWriteAndNoExecute wrapper
// should be used to prevent bugs from not pairing up the calls properly.

#ifndef IPHONEOS
// Allows a thread to write to executable memory, but not execute the data.
void JITPageWriteEnableExecuteDisable();
// Allows a thread to execute memory allocated for execution, but not write to it.
void JITPageWriteDisableExecuteEnable();
// RAII Wrapper around JITPageWrite*Execute*(). When this is in scope the thread can
// write to executable memory but not execute it.
struct ScopedJITPageWriteAndNoExecute
{
  ScopedJITPageWriteAndNoExecute(u8*) { JITPageWriteEnableExecuteDisable(); }
  ~ScopedJITPageWriteAndNoExecute() { JITPageWriteDisableExecuteEnable(); }
};
#else
void JITPageWriteEnableExecuteDisable(void* ptr);
void JITPageWriteDisableExecuteEnable(void* ptr);

struct ScopedJITPageWriteAndNoExecute
{
  ScopedJITPageWriteAndNoExecute(u8* region)
  {
    ptr = reinterpret_cast<void*>(region);
    JITPageWriteEnableExecuteDisable(ptr);
  }
  ~ScopedJITPageWriteAndNoExecute() { JITPageWriteDisableExecuteEnable(ptr); }

  void* ptr;
};
#endif
void* AllocateMemoryPages(size_t size);
bool FreeMemoryPages(void* ptr, size_t size);
void* AllocateAlignedMemory(size_t size, size_t alignment);
void FreeAlignedMemory(void* ptr);
bool ReadProtectMemory(void* ptr, size_t size);
bool WriteProtectMemory(void* ptr, size_t size, bool executable = false);
bool UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute = false);
size_t MemPhysical();

}  // namespace Common
