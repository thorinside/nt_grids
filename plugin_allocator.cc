// Simple bump allocator to satisfy operator new on disting EX without pulling full libstdc++.
// 8 KiB static heap (adjust if ever exhausted).
#include <cstddef>
#include <stdint.h>

namespace
{
  constexpr std::size_t kHeapSize = 8 * 1024; // 8 KiB
  alignas(8) static uint8_t g_heap[kHeapSize];
  static std::size_t g_offset = 0;
}

void *operator new(std::size_t size) noexcept
{
  size = (size + 7) & ~static_cast<std::size_t>(7); // align 8
  if (g_offset + size > kHeapSize)
    return nullptr; // out of arena
  void *ptr = &g_heap[g_offset];
  g_offset += size;
  return ptr;
}

void operator delete(void * /*ptr*/) noexcept {}
void operator delete(void * /*ptr*/, std::size_t) noexcept {}