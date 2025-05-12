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

#ifndef NT_GRIDS_RESOURCES_H_
#define NT_GRIDS_RESOURCES_H_

#include <stdint.h>

namespace nt_grids_port
{

  // Constants from grids/pattern_generator.h (or relevant to data usage)
  const uint8_t kNumParts = 3;
  const uint8_t kStepsPerPattern = 32;

  // Sizes from grids/resources.h
  const int LUT_RES_EUCLIDEAN_SIZE = 1024;
  const int NODE_DATA_SIZE = 96; // All node_X arrays are 96 bytes

  // External declarations for data arrays (definitions will be in .cc file)
  extern const uint32_t lut_res_euclidean[LUT_RES_EUCLIDEAN_SIZE];

  extern const uint8_t node_0[NODE_DATA_SIZE];
  extern const uint8_t node_1[NODE_DATA_SIZE];
  extern const uint8_t node_2[NODE_DATA_SIZE];
  extern const uint8_t node_3[NODE_DATA_SIZE];
  extern const uint8_t node_4[NODE_DATA_SIZE];
  extern const uint8_t node_5[NODE_DATA_SIZE];
  extern const uint8_t node_6[NODE_DATA_SIZE];
  extern const uint8_t node_7[NODE_DATA_SIZE];
  extern const uint8_t node_8[NODE_DATA_SIZE];
  extern const uint8_t node_9[NODE_DATA_SIZE];
  extern const uint8_t node_10[NODE_DATA_SIZE];
  extern const uint8_t node_11[NODE_DATA_SIZE];
  extern const uint8_t node_12[NODE_DATA_SIZE];
  extern const uint8_t node_13[NODE_DATA_SIZE];
  extern const uint8_t node_14[NODE_DATA_SIZE];
  extern const uint8_t node_15[NODE_DATA_SIZE];
  extern const uint8_t node_16[NODE_DATA_SIZE];
  extern const uint8_t node_17[NODE_DATA_SIZE];
  extern const uint8_t node_18[NODE_DATA_SIZE];
  extern const uint8_t node_19[NODE_DATA_SIZE];
  extern const uint8_t node_20[NODE_DATA_SIZE];
  extern const uint8_t node_21[NODE_DATA_SIZE];
  extern const uint8_t node_22[NODE_DATA_SIZE];
  extern const uint8_t node_23[NODE_DATA_SIZE];
  extern const uint8_t node_24[NODE_DATA_SIZE];

  // Drum map structure as used in pattern_generator.cc
  // This provides a C++ way to access the node data similar to the original drum_map
  // The actual node_X arrays are defined above and will be in nt_grids_resources.cc
  struct DrumMapAccess
  {
    static const uint8_t *const drum_map_ptr[5][5];
  };

} // namespace nt_grids_port

#endif // NT_GRIDS_RESOURCES_H_