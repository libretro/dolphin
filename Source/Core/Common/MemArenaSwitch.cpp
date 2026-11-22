// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(__LIBRETRO__) && defined(__SWITCH2__)

#include "Common/MemArena.h"

#include <cstring>
#include "Common/MemoryUtil.h"

namespace Common
{
MemArena::MemArena() = default;
MemArena::~MemArena() = default;

void MemArena::GrabSHMSegment(size_t size, std::string_view)
{
  m_backing = static_cast<u8*>(AllocateMemoryPages(size));
  m_backing_size = m_backing ? size : 0;
}

void MemArena::ReleaseSHMSegment()
{
  FreeMemoryPages(m_backing, m_backing_size);
  m_backing = nullptr;
  m_backing_size = 0;
}

void* MemArena::CreateView(s64 offset, size_t size)
{
  if (!m_backing || offset < 0 || static_cast<u64>(offset) > m_backing_size ||
      size > m_backing_size - static_cast<size_t>(offset))
    return nullptr;

  // Views borrow the backing allocation until ReleaseSHMSegment.
  return m_backing + offset;
}

void MemArena::ReleaseView(void*, size_t) {}
u8* MemArena::ReserveMemoryRegion(size_t) { return nullptr; }
void MemArena::ReleaseMemoryRegion() {}
void* MemArena::MapInMemoryRegion(s64, size_t, void*, bool) { return nullptr; }
bool MemArena::ChangeMappingProtection(void*, size_t, bool) { return false; }
void MemArena::UnmapFromMemoryRegion(void*, size_t) {}
size_t MemArena::GetPageSize() const { return 0x1000; }

LazyMemoryRegion::LazyMemoryRegion() = default;
LazyMemoryRegion::~LazyMemoryRegion() { Release(); }

void* LazyMemoryRegion::Create(size_t)
{
  // No demand-paged mappings; callers must use their bounded fallback.
  return nullptr;
}

void LazyMemoryRegion::Clear() {}
void LazyMemoryRegion::Release() {}
}  // namespace Common

#endif
