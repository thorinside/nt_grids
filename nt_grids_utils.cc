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

#include "nt_grids_utils.h"
// #include <stdlib.h> // No longer needed here if Random::Init doesn't use rand/srand

namespace nt_grids_port
{

  // Static member definitions are still needed if they are not const static inline / constexpr in C++17
  uint16_t Random::rng_state_ = 0xACE1; // Initial arbitrary seed, Init() will override if called
  bool Random::s_seeded_ = false;

  // All method definitions below were previously causing redefinition errors
  // as they are now defined inline in the header. They are removed.

  /*
  void Random::Init()
  {
    // This logic is now in the header's inline definition
  }

  void Random::Seed(uint16_t seed)
  {
    // This logic is now in the header's inline definition
  }

  uint16_t Random::GetWord()
  {
    // This logic is now in the header's inline definition
  }

  uint8_t Random::GetByte()
  {
    // This logic is now in the header's inline definition
  }

  uint8_t Random::GetControlByte()
  {
    // This logic is now in the header's inline definition
  }
  */

  // --- Other utility functions can remain if they are not inline in the header ---
  // e.g. U8Mix, U8U8MulShift8, U8U8Mul if they were not inline in .h (but they are)

} // namespace nt_grids_port