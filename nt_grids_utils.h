// Copyright (C) 2012 Emilie Gillet.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// Based on the original Grids by Emilie Gillet.

#ifndef NT_GRIDS_UTILS_H_
#define NT_GRIDS_UTILS_H_

#include <stdint.h> // For uint8_t, uint16_t
// #include <stdlib.h> // No longer needed if not using rand()/srand()
#include <cstring>         // For memset, C++ style
#include "distingnt/api.h" // For NT_getCpuCycleCount()

// Copied from avrlib/base.h - C++11 style using = delete
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName &) = delete;     \
  TypeName &operator=(const TypeName &) = delete

namespace nt_grids_port
{

  // Utility functions (C++ versions of avrlib/op.h functions)
  inline uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t balance)
  {
    return static_cast<uint8_t>(((uint16_t)a * (255 - balance) + (uint16_t)b * balance) >> 8);
  }

  inline uint8_t U8U8MulShift8(uint8_t a, uint8_t b)
  {
    return static_cast<uint8_t>(((uint16_t)a * b) >> 8);
  }

  inline uint16_t U8U8Mul(uint8_t a, uint8_t b)
  {
    return (uint16_t)a * b;
  }

  // Ported Random class from avrlib/random.h
  class Random
  {
  public:
    static void Init()
    {
      if (!s_seeded_)
      {
        rng_state_ = 0xACE1; // Use a fixed seed, bypassing NT_getCpuCycleCount() for now
        // rng_state_ = static_cast<uint16_t>(NT_getCpuCycleCount() & 0xFFFF); // Use lower 16 bits of CPU cycle count
        // if (rng_state_ == 0)
        //   rng_state_ = 0xACE1; // Ensure seed is not zero, as LFSR can get stuck
        s_seeded_ = true;
      }
    }

    static void Seed(uint16_t seed)
    {
      rng_state_ = seed;
      s_seeded_ = true; // Consider if explicit seeding should override srand approach
    }

    static void Update()
    {
      // Galois LFSR with feedback polynomial = x^16 + x^14 + x^13 + x^11.
      // Period: 65535.
      rng_state_ = (rng_state_ >> 1) ^ (-(rng_state_ & 1) & 0xb400);
    }

    static inline uint16_t state() { return rng_state_; }

    static inline uint8_t state_msb()
    {
      return static_cast<uint8_t>(rng_state_ >> 8);
    }

    static inline uint8_t GetByte()
    {
      Update();
      return state_msb();
    }

    static inline uint8_t GetControlByte()
    {
      Update();
      return static_cast<uint8_t>((state() >> 8) & 0xFF);
    }

    static inline uint16_t GetWord()
    {
      Update();
      return state();
    }

  private:
    static uint16_t rng_state_;
    static bool s_seeded_; // To ensure srand is called only once by default Init

    DISALLOW_COPY_AND_ASSIGN(Random);
  };

} // namespace nt_grids_port

#endif // NT_GRIDS_UTILS_H_