// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(__LIBRETRO__) && defined(__SWITCH2__)

#include "Common/MemoryUtil.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <malloc.h>
#include <kaihen.h>

#include "Common/MsgHandler.h"

namespace Common
{
namespace
{
struct CodeAllocation
{
  std::atomic<unsigned> state{0};
  std::atomic<uintptr_t> base{0};
  std::atomic<size_t> size{0};
  Jit code{};
};

// Match the RO service's maximum number of live JIT mappings.
std::array<CodeAllocation, 512> s_code;

CodeAllocation* FindCode(void* ptr, size_t size)
{
  const auto address = reinterpret_cast<uintptr_t>(ptr);
  for (auto& allocation : s_code)
  {
    const auto base = allocation.base.load(std::memory_order_acquire);
    const auto capacity = allocation.size.load(std::memory_order_relaxed);
    if (base && address >= base && address - base <= capacity &&
        size <= capacity - (address - base))
      return &allocation;
  }
  return nullptr;
}

[[noreturn]] void JitFailure(const char* operation, Result result)
{
  PanicAlertFmt("Switch 2 JIT {} failed: {:08x}", operation, result);
  std::abort();
}
}

void* AllocateExecutableMemory(size_t size)
{
  for (auto& allocation : s_code)
  {
    unsigned expected = 0;

    if (!allocation.state.compare_exchange_strong(expected, 1, std::memory_order_acquire))
      continue;

    const Result result = jitCreate(&allocation.code, size);

    if (R_FAILED(result))
    {
      allocation.state.store(0, std::memory_order_release);
      JitFailure("allocation", result);
    }

    if (allocation.code.type != JitType_CodeMemory &&
        allocation.code.rw_addr == allocation.code.src_addr)
      JitFailure("persistent writable mapping", MAKERESULT(Module_Libnx, LibnxError_JitUnavailable));
    allocation.size.store(allocation.code.size, std::memory_order_relaxed);
    allocation.base.store(reinterpret_cast<uintptr_t>(allocation.code.rx_addr),
                          std::memory_order_release);

    return allocation.code.rx_addr;
  }

  JitFailure("allocation limit", MAKERESULT(Module_Libnx, LibnxError_OutOfMemory));
}

ptrdiff_t AllocateWritableRegionAndGetDiff(void* ptr, size_t size)
{
  auto* allocation = FindCode(ptr, size);

  if (!allocation)
    JitFailure("writable mapping lookup", MAKERESULT(Module_Libnx, LibnxError_BadInput));

  return static_cast<ptrdiff_t>(reinterpret_cast<uintptr_t>(allocation->code.rw_addr) -
                                reinterpret_cast<uintptr_t>(allocation->code.rx_addr));
}

void PublishExecutableMemory(void* ptr, size_t size)
{
  if (!size)
    return;

  auto* allocation = FindCode(ptr, size);

  if (!allocation)
    JitFailure("publication lookup", MAKERESULT(Module_Libnx, LibnxError_BadInput));
  auto& code = allocation->code;

  const size_t offset = reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(code.rx_addr);
  void* bytes = static_cast<u8*>(code.rw_addr) + offset;

  if (code.type == JitType_CodeMemory)
    armDCacheFlush(bytes, size);
  else
  {
    // RO owns the writable alias; publication must finish before this code executes.
    const Result result = kaihenPublishJitAlias(&code, offset, bytes, size);

    if (R_FAILED(result))
      JitFailure("publication", result);
  }

  armICacheInvalidate(ptr, size);
}

void FreeWritableRegion(void*, size_t, ptrdiff_t)
{
  // jitClose releases both mappings together.
}

void FreeExecutableMemory(void* ptr, size_t size)
{
  if (!ptr)
    return;

  auto* allocation = FindCode(ptr, size);

  if (!allocation)
    JitFailure("release lookup", MAKERESULT(Module_Libnx, LibnxError_BadInput));

  allocation->base.store(0, std::memory_order_release);
  const Result result = jitClose(&allocation->code);

  if (R_FAILED(result))
    JitFailure("release", result);

  allocation->state.store(0, std::memory_order_release);
}

void JITPageWriteEnableExecuteDisable() {}
void JITPageWriteDisableExecuteEnable() {}

void* AllocateMemoryPages(size_t size)
{
  if (!size || size > SIZE_MAX - 0xfff)
    return nullptr;

  return memalign(0x1000, (size + 0xfff) & ~size_t{0xfff});
}

bool FreeMemoryPages(void* ptr, size_t size)
{
  if (!ptr)
    return true;

  if (R_FAILED(svcSetMemoryPermission(ptr, (size + 0xfff) & ~size_t{0xfff}, Perm_Rw)))
    return false;

  std::free(ptr);

  return true;
}
void* AllocateAlignedMemory(size_t size, size_t alignment) { return memalign(alignment, size); }
void FreeAlignedMemory(void* ptr) { std::free(ptr); }

bool ReadProtectMemory(void* ptr, size_t size)
{
  return R_SUCCEEDED(svcSetMemoryPermission(ptr, size, Perm_None));
}

bool WriteProtectMemory(void* ptr, size_t size, bool executable)
{
  if (executable)
    return FindCode(ptr, size) != nullptr;

  return R_SUCCEEDED(svcSetMemoryPermission(ptr, size, Perm_R));
}

bool UnWriteProtectMemory(void* ptr, size_t size, bool executable)
{
  if (executable)
    return FindCode(ptr, size) != nullptr;

  return R_SUCCEEDED(svcSetMemoryPermission(ptr, size, Perm_Rw));
}

size_t MemPhysical()
{
  u64 size = 0;
  svcGetInfo(&size, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);

  return size;
}
}  // namespace Common

#endif
