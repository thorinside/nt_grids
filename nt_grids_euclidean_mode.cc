#include "nt_grids_euclidean_mode.h"
#include "nt_grids.h"           // For NtGridsAlgorithm, ParameterIndex, etc.
#include "nt_grids_resources.h" // For nt_grids_port::lut_res_euclidean and LUT_RES_EUCLIDEAN_SIZE
#include <cstring>
#include <cstdio> // For snprintf

#ifdef TESTING_BUILD
#include "tests/mock_nt_platform_adapter.h" // For dynamic_cast check
#endif

// Manual MIN/MAX macros if not available
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

EuclideanModeStrategy::EuclideanModeStrategy(NtGridsAlgorithm *self_algo_for_init)
{
  if (!self_algo_for_init)
    return; // Should not happen if used correctly

  m_euclidean_controls_length = (self_algo_for_init->v[kParamEuclideanControlsLength] != 0);

  // Removed pot configuration from constructor
  // for (int i = 0; i < 3; ++i)
  // {
  //   ParameterIndex primary_idx;
  //   ParameterIndex alternate_idx;
  //   bool has_alternate;
  //   float primary_scale;
  //   float alternate_scale;
  //   // Call the class's own determinePotConfig to get the values
  //   this->determinePotConfig(self_algo_for_init, i, primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
  //   // Now configure the actual TakeoverPot object using these values
  //   self_algo_for_init->m_pots[i].configure(primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
  // }
}

// Handle encoder input for Euclidean mode (Encoder R: Chaos Amount)
void EuclideanModeStrategy::handleEncoderInput(NtGridsAlgorithm *self, const _NT_uiData &data, int encoder_idx)
{
  if (!self)
    return;

  int encoder_delta = data.encoders[encoder_idx];
  if (encoder_delta == 0)
    return;

  ParameterIndex target_param = kParamMode; // Default, should be overwritten or result in no action

  if (encoder_idx == 0) // Left Encoder
  {
    return; // Left encoder does nothing in Euclidean mode
  }
  else if (encoder_idx == 1) // Right Encoder
  {
    target_param = kParamChaosAmount;
  }
  else
  {
    return; // Invalid encoder index
  }

  const _NT_parameter *param_def = self->m_platform_adapter.getParameterDefinition(target_param);
  if (!param_def)
    return;

  int32_t current_val = self->v[target_param];
  int32_t new_val = current_val + encoder_delta;
  // For Chaos Amount, a larger step might be desired, e.g., steps of 5 as per README
  // However, the generic encoder handling here is 1:1. If steps of 5 are strictly needed,
  // this logic would need to be more specific for kParamChaosAmount.
  // For now, using direct encoder_delta.
  // If steps of 5 were desired: new_val = current_val + (encoder_delta * 5);

  new_val = MAX(param_def->min, MIN(param_def->max, new_val));

  if (new_val != current_val)
  {
    uint32_t alg_idx = self->m_platform_adapter.getAlgorithmIndex(self);
    uint32_t param_offset = self->m_platform_adapter.getParameterOffset();
    self->m_platform_adapter.setParameterFromUi(alg_idx, target_param + param_offset, new_val);
  }
}

// Process pots for Euclidean mode (Fill/Length, toggling with m_euclidean_controls_length)
void EuclideanModeStrategy::processPotsUI(NtGridsAlgorithm *self, const _NT_uiData &data)
{
  // const float max_param_255 = 255.0f; // Unused locally
  // const float max_param_32 = 32.0f;   // Unused locally

  for (int i = 0; i < 3; ++i) // Pot 0, 1, 2
  {
    // ParameterIndex primary_param_idx, alternate_param_idx;
    // bool has_alternate;
    // float primary_scale, alternate_scale;

    // This function determines what parameter each pot controls (primary and optionally alternate)
    // and the scaling for the pot's physical value to the parameter's logical value.
    // this->determinePotConfig(self, i, primary_param_idx, alternate_param_idx, has_alternate, primary_scale, alternate_scale); // MOVED to onModeActivated and button handler

    // The pot is configured based on the strategy's state.
    // self->m_pots[i].configure(primary_param_idx, alternate_param_idx, has_alternate, primary_scale, alternate_scale); // MOVED to onModeActivated and button handler

    // Then update is called.
    self->m_pots[i].update(data);
  }

  // Handle Pot R button for toggling Length/Fill control focus in Euclidean mode
  if ((data.buttons & kNT_potButtonR) && !(data.lastButtons & kNT_potButtonR))
  {
    m_euclidean_controls_length = !m_euclidean_controls_length;
    int32_t new_param_val = m_euclidean_controls_length ? 255 : 0;

    if (self) // Only check for self, m_platform_adapter is guaranteed if self is valid
    {
      uint32_t alg_idx = self->m_platform_adapter.getAlgorithmIndex(self);
      uint32_t param_offset = self->m_platform_adapter.getParameterOffset();
      self->m_platform_adapter.setParameterFromUi(alg_idx, kParamEuclideanControlsLength + param_offset, new_param_val);
    }

    for (int i = 0; i < 3; ++i)
    {
      ParameterIndex primary_idx, alt_idx;
      bool has_alt;
      float p_scale, a_scale;
      determinePotConfig(self, i, primary_idx, alt_idx, has_alt, p_scale, a_scale);
      self->m_pots[i].configure(primary_idx, alt_idx, has_alt, p_scale, a_scale);
      // Pots also need their takeover state reset for the new primary param
      self->m_pots[i].resetTakeoverForNewPrimary(self->v[primary_idx] / (p_scale > 0 ? p_scale : 1.0f));
    }
  }
}

// Set up initial pot positions for Euclidean mode (Fill 1-3 or Length 1-3)
void EuclideanModeStrategy::setupTakeoverPots(NtGridsAlgorithm *self, _NT_float3 &pots)
{
  for (int i = 0; i < 3; ++i)
  {
    ParameterIndex primary_param_idx = m_euclidean_controls_length ? (ParameterIndex)(kParamEuclideanLength1 + i * 3) : (ParameterIndex)(kParamEuclideanFill1 + i * 3);
    // Corrected indexing: each channel's params are 3 apart (L, F, S)
    // However, the original intent here might be that pot i controls channel i,
    // and m_euclidean_controls_length switches if it's Length or Fill.
    // The kParamEuclideanLength1 + i assumes Length1, Length2, Length3 are contiguous, which they are NOT in ParameterIndex enum.
    // kParamEuclideanLength1, kParamEuclideanFill1, kParamEuclideanShift1, kParamEuclideanLength2 ...
    // So, for pot 0 controlling channel 1 (Length1 or Fill1):
    // primary_param_idx = m_euclidean_controls_length ? kParamEuclideanLength1 : kParamEuclideanFill1;
    // For pot 1 controlling channel 2 (Length2 or Fill2):
    // primary_param_idx = m_euclidean_controls_length ? kParamEuclideanLength2 : kParamEuclideanFill2;
    // For pot 2 controlling channel 3 (Length3 or Fill3):
    // primary_param_idx = m_euclidean_controls_length ? kParamEuclideanLength3 : kParamEuclideanFill3;

    // Let's use a switch for clarity based on i (pot_index)
    switch (i)
    {
    case 0: // Pot L, Channel 1
      primary_param_idx = m_euclidean_controls_length ? kParamEuclideanLength1 : kParamEuclideanFill1;
      break;
    case 1: // Pot C, Channel 2
      primary_param_idx = m_euclidean_controls_length ? kParamEuclideanLength2 : kParamEuclideanFill2;
      break;
    case 2: // Pot R, Channel 3
      primary_param_idx = m_euclidean_controls_length ? kParamEuclideanLength3 : kParamEuclideanFill3;
      break;
    default: // Should not happen
      primary_param_idx = kParamEuclideanLength1;
      break;
    }

    float primary_scale = ((primary_param_idx == kParamEuclideanLength1 || primary_param_idx == kParamEuclideanLength2 || primary_param_idx == kParamEuclideanLength3) || (primary_param_idx == kParamEuclideanFill1 || primary_param_idx == kParamEuclideanFill2 || primary_param_idx == kParamEuclideanFill3 && self->v[primary_param_idx] > 16) // If fill somehow gets > 16, scale by 255. Else 16.
                           )
                              ? 16.0f
                              : 16.0f; // Lengths (1-16), Fills (0-16). Max value is 16. Let's assume scale of 16 for simplicity.
                                       // Original: primary_scale = m_euclidean_controls_length ? 32.0f : 255.0f; -- This seems wrong. Lengths are 1-16, Fills 0-16.

    pots[i] = (primary_scale > 0) ? ((float)self->v[primary_param_idx] / primary_scale) : 0.0f;
    // self->m_pots[i].syncPhysicalValue(pots[i]); // REMOVED
  }
}

// Draw Euclidean mode UI (length/fill, chaos)
void EuclideanModeStrategy::drawModeUI(NtGridsAlgorithm *self, int y_start, _NT_textSize text_size, int line_spacing, char *buffer)
{
  if (!self)
    return;

  ParameterIndex current_lengths[] = {kParamEuclideanLength1, kParamEuclideanLength2, kParamEuclideanLength3};
  ParameterIndex current_fills[] = {kParamEuclideanFill1, kParamEuclideanFill2, kParamEuclideanFill3};

  char val_buf[8]; // Buffer for NT_intToString output

  uint8_t active_color = 15;
  uint8_t inactive_color = 8;
  uint8_t len_color_part;  // Color for "L1:val"
  uint8_t fill_color_part; // Color for ":val" of fill

  if (m_euclidean_controls_length)
  {
    len_color_part = active_color;
    fill_color_part = inactive_color;
  }
  else
  {
    len_color_part = inactive_color;
    fill_color_part = active_color;
  }

  const int SCREEN_WIDTH_PX = 256;
  const int CHAR_WIDTH_PX = 8; // Assuming text_size makes chars 8px wide
  // const int SCREEN_WIDTH_CHARS = SCREEN_WIDTH_PX / CHAR_WIDTH_PX; // 32

  // char full_line_for_width_calc[MAX_PARAM_STRING_LENGTH * 6 + 10]; // Generous buffer for width calculation - REMOVED
  int total_line_chars = 0;
  // full_line_for_width_calc[0] = '\0'; // REMOVED

  // --- Stage 1: Calculate total character length of the line for centering ---
  for (int i = 0; i < 3; ++i)
  {
    // Lx:
    total_line_chars += 3;

    // Length value
    NT_intToString(val_buf, self->v[current_lengths[i]]);
    int len_val_str_len = 0;
    while (val_buf[len_val_str_len] != '\0' && len_val_str_len < 7)
      len_val_str_len++;
    total_line_chars += len_val_str_len;

    // :
    total_line_chars += 1;

    // Fill value
    NT_intToString(val_buf, self->v[current_fills[i]]);
    int fill_val_str_len = 0;
    while (val_buf[fill_val_str_len] != '\0' && fill_val_str_len < 7)
      fill_val_str_len++;
    total_line_chars += fill_val_str_len;

    if (i < 2)
    {
      total_line_chars += 3; // "   " separator
    }
  }

  int current_draw_x = MAX(0, (SCREEN_WIDTH_PX - (total_line_chars * CHAR_WIDTH_PX)) / 2);

  // --- Stage 2: Draw the line segment by segment with colors ---
  char segment_buf[12]; // Buffer for small segments like "L1:", ":16", ":4"

  for (int i = 0; i < 3; ++i)
  {
    int seg_pos = 0;

    // Part 1: "Lx:"
    segment_buf[seg_pos++] = 'L';
    segment_buf[seg_pos++] = (char)('1' + i);
    segment_buf[seg_pos++] = ':';
    segment_buf[seg_pos] = '\0';
    self->m_platform_adapter.drawText(current_draw_x, y_start, segment_buf, len_color_part, kNT_textLeft, text_size);
    current_draw_x += seg_pos * CHAR_WIDTH_PX;
    seg_pos = 0;

    // Part 2: Length value
    NT_intToString(val_buf, self->v[current_lengths[i]]);
    // No need to copy val_buf to segment_buf, drawText can take val_buf directly
    self->m_platform_adapter.drawText(current_draw_x, y_start, val_buf, len_color_part, kNT_textLeft, text_size);
    int len_val_str_len = 0;
    while (val_buf[len_val_str_len] != '\0' && len_val_str_len < 7)
      len_val_str_len++;
    current_draw_x += len_val_str_len * CHAR_WIDTH_PX;

    // Part 3: ":" (for fill)
    segment_buf[seg_pos++] = ':';
    segment_buf[seg_pos] = '\0';
    self->m_platform_adapter.drawText(current_draw_x, y_start, segment_buf, fill_color_part, kNT_textLeft, text_size);
    current_draw_x += seg_pos * CHAR_WIDTH_PX;
    seg_pos = 0;

    // Part 4: Fill value
    NT_intToString(val_buf, self->v[current_fills[i]]);
    self->m_platform_adapter.drawText(current_draw_x, y_start, val_buf, fill_color_part, kNT_textLeft, text_size);
    int fill_val_str_len = 0;
    while (val_buf[fill_val_str_len] != '\0' && fill_val_str_len < 7)
      fill_val_str_len++;
    current_draw_x += fill_val_str_len * CHAR_WIDTH_PX;

    if (i < 2) // Add "   " separator
    {
      // Draw spaces one by one if drawText("   ") causes issues or for exact spacing
      segment_buf[0] = ' ';
      segment_buf[1] = ' ';
      segment_buf[2] = ' ';
      segment_buf[3] = '\0';
      self->m_platform_adapter.drawText(current_draw_x, y_start, segment_buf, inactive_color, kNT_textLeft, text_size);
      current_draw_x += 3 * CHAR_WIDTH_PX;
    }
  }

  // Second line: Chaos display
  int chaos_y = y_start + line_spacing;

  self->m_platform_adapter.drawText(200, chaos_y, "Chaos:", 15, kNT_textLeft, text_size);
  bool chaos_enabled = self->v[kParamChaosEnable];
  if (chaos_enabled)
  {
    NT_intToString(buffer, self->v[kParamChaosAmount]); // Use the passed-in buffer from nt_grids_draw
    self->m_platform_adapter.drawText(255, chaos_y, buffer, 15, kNT_textRight, text_size);
  }
  else
  {
    self->m_platform_adapter.drawText(255, chaos_y, "Off", 15, kNT_textRight, text_size);
  }
}

// Called when Euclidean mode is activated
void EuclideanModeStrategy::onModeActivated(NtGridsAlgorithm *self)
{
  // TESTING_BUILD block removed
  if (!self)
    return;

  // Configure pots when mode is activated
  for (int i = 0; i < 3; ++i)
  {
    ParameterIndex primary_idx;
    ParameterIndex alternate_idx;
    bool has_alternate;
    float primary_scale;
    float alternate_scale;
    this->determinePotConfig(self, i, primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
    self->m_pots[i].configure(primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
  }
  // Note: setupTakeoverPots might also need to be called here or ensured it's called after this
  // if onModeActivated is called outside of the initial UI setup flow.
  // Currently, nt_grids_construct calls onModeActivated, then later setupUi is called when UI is entered.
  // And parameterChanged also calls onModeActivated, which should be fine as setupUi will follow if UI is active.
}

// Called when Euclidean mode is deactivated
void EuclideanModeStrategy::onModeDeactivated(NtGridsAlgorithm * /*self*/)
{
  // No special state to reset for Euclidean mode
}

// Map pots to Euclidean mode parameters (Fill/Length, toggling with m_euclidean_controls_length)
void EuclideanModeStrategy::determinePotConfig(NtGridsAlgorithm *self, int pot_index, ParameterIndex &primary_idx, ParameterIndex &alternate_idx, bool &has_alternate, float &primary_scale, float &alternate_scale)
{
  bool pots_control_length = m_euclidean_controls_length; // Use member variable

  switch (pot_index)
  {
  case 0: // Pot L, Channel 1
    primary_idx = pots_control_length ? kParamEuclideanLength1 : kParamEuclideanFill1;
    break;
  case 1: // Pot C, Channel 2
    primary_idx = pots_control_length ? kParamEuclideanLength2 : kParamEuclideanFill2;
    break;
  case 2: // Pot R, Channel 3
    primary_idx = pots_control_length ? kParamEuclideanLength3 : kParamEuclideanFill3;
    break;
  default: // Should not happen
    primary_idx = kParamEuclideanLength1;
    break;
  }

  primary_scale = 16.0f; // All Euclidean Length/Fill params have max 16

  // In Euclidean mode, pots don't use button-toggled alternate parameters in the same way as Drum mode's Chaos.
  // The "alternate" function (Length vs Fill) is a mode-wide toggle for all three pots.
  alternate_idx = primary_idx; // Set to same as primary, or kParamNone if that's more appropriate for TakeoverPot logic when has_alternate is false.
  alternate_scale = primary_scale;
  has_alternate = false;
}

// Return mode name
const char *EuclideanModeStrategy::getModeName() const
{
  return "Euclidean";
}