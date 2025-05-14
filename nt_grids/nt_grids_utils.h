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

#include <stdint.h>
#include <cstring>
#include "distingnt/api.h"

namespace nt_grids_port
{

  class Random
  {
  public:
    static void Init();
    static uint16_t Get();

  private:
    static uint16_t rng_state_;
    static bool s_seeded_;
  };

} // namespace nt_grids_port

#endif // NT_GRIDS_UTILS_H_