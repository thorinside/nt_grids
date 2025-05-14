#include "nt_grids_euclidean_mode.h"
#include "nt_grids.h"           // For NtGridsAlgorithm, ParameterIndex, etc.
#include "nt_grids_resources.h" // For nt_grids_port::lut_res_euclidean and LUT_RES_EUCLIDEAN_SIZE
#include <cstring>
#include <cstdio> // For snprintf

#ifdef TESTING_BUILD
#include "tests/mock_nt_platform_adapter.h" // For dynamic_cast check
#endif

EuclideanModeStrategy::EuclideanModeStrategy(NtGridsAlgorithm *self_algo_for_init)
{
  if (!self_algo_for_init)
    return; // Should not happen if used correctly

  m_euclidean_controls_length = (self_algo_for_init->v[kParamEuclideanControlsLength] != 0);

#ifdef TESTING_BUILD
  MockNtPlatformAdapter *mock_adapter = dynamic_cast<MockNtPlatformAdapter *>(self_algo_for_init->m_platform_adapter);
  if (mock_adapter)
  {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "EuclideanModeStrategy CONSTRUCTOR: self->v[kECL] = %d, m_ECL set to %s",
             self_algo_for_init->v[kParamEuclideanControlsLength], m_euclidean_controls_length ? "true" : "false");
    mock_adapter->logMessage(buffer);
  }
#endif

  for (int i = 0; i < 3; ++i)
  {
    ParameterIndex primary_idx;
    ParameterIndex alternate_idx;
    bool has_alternate;
    float primary_scale;
    float alternate_scale;
    // Call the class's own determinePotConfig to get the values
    this->determinePotConfig(self_algo_for_init, i, primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
    // Now configure the actual TakeoverPot object using these values
    self_algo_for_init->m_pots[i].configure(primary_idx, alternate_idx, has_alternate, primary_scale, alternate_scale);
  }
}

// Handle encoder input for Euclidean mode (Encoder R: Chaos Amount)
void EuclideanModeStrategy::handleEncoderInput(NtGridsAlgorithm *self, const _NT_uiData &data, int encoder_index)
{
  if (!self || !self->m_platform_adapter)
    return;
  int32_t encoder_delta = data.encoders[encoder_index];
  if (encoder_delta == 0)
    return;
  if (encoder_index == 1)
  { // Encoder R
    ParameterIndex param_idx_to_change = kParamChaosAmount;
    int step_multiplier = 5;
    const _NT_parameter *param_def = self->m_platform_adapter->getParameterDefinition(param_idx_to_change);
    if (!param_def)
      return;
    int current_val = self->v[param_idx_to_change];
    int min_val = param_def->min;
    int max_val = param_def->max;
    int new_val = current_val + encoder_delta * step_multiplier;
    if (new_val < min_val)
      new_val = min_val;
    if (new_val > max_val)
      new_val = max_val;
    uint32_t alg_idx = self->m_platform_adapter->getAlgorithmIndex(self);
    uint32_t param_offset = self->m_platform_adapter->getParameterOffset();
    self->m_platform_adapter->setParameterFromUi(alg_idx, param_idx_to_change + param_offset, new_val);
  }
  // Encoder L does nothing in Euclidean mode
}

// Process pots for Euclidean mode (Fill/Length, toggling with m_euclidean_controls_length)
void EuclideanModeStrategy::processPotsUI(NtGridsAlgorithm *self, const _NT_uiData &data)
{
  // const float max_param_255 = 255.0f; // Unused locally
  // const float max_param_32 = 32.0f;   // Unused locally

  for (int i = 0; i < 3; ++i) // Pot 0, 1, 2
  {
    ParameterIndex primary_param_idx, alternate_param_idx;
    bool has_alternate;
    float primary_scale, alternate_scale;

    // This function determines what parameter each pot controls (primary and optionally alternate)
    // and the scaling for the pot's physical value to the parameter's logical value.
    this->determinePotConfig(self, i, primary_param_idx, alternate_param_idx, has_alternate, primary_scale, alternate_scale);

    // The pot is configured based on the strategy's state.
    self->m_pots[i].configure(primary_param_idx, alternate_param_idx, has_alternate, primary_scale, alternate_scale);

    // Then update is called.
    self->m_pots[i].update(data);
  }

  // Handle Pot R button for toggling Length/Fill control focus in Euclidean mode
  if ((data.buttons & kNT_potButtonR) && !(data.lastButtons & kNT_potButtonR))
  {
    m_euclidean_controls_length = !m_euclidean_controls_length;
    int32_t new_param_val = m_euclidean_controls_length ? 255 : 0;

    if (self && self->m_platform_adapter)
    { // Safety check
      uint32_t alg_idx = self->m_platform_adapter->getAlgorithmIndex(self);
      uint32_t param_offset = self->m_platform_adapter->getParameterOffset();
      self->m_platform_adapter->setParameterFromUi(alg_idx, kParamEuclideanControlsLength + param_offset, new_param_val);
    }

#ifdef TESTING_BUILD
    MockNtPlatformAdapter *mock_adapter = dynamic_cast<MockNtPlatformAdapter *>(self->m_platform_adapter);
    if (self && mock_adapter)
    {
      char buffer[100];
      snprintf(buffer, sizeof(buffer), "EuclideanModeStrategy: PotR pressed. m_ECL toggled to %s. Param %d updated.",
               m_euclidean_controls_length ? "true (Length)" : "false (Fill)", kParamEuclideanControlsLength);
      mock_adapter->logMessage(buffer);
    }
#endif
    // After toggling, the pots need to be reconfigured for their new roles immediately
    // This loop re-runs the configure logic based on the new m_euclidean_controls_length state.
    for (int i = 0; i < 3; ++i)
    {
      ParameterIndex primary_idx, alt_idx; // Vars to receive config
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
    ParameterIndex primary_param_idx = m_euclidean_controls_length ? (ParameterIndex)(kParamEuclideanLength1 + i) : (ParameterIndex)(kParamEuclideanFill1 + i);
    float primary_scale = m_euclidean_controls_length ? 32.0f : 255.0f;
    pots[i] = (primary_scale > 0) ? ((float)self->v[primary_param_idx] / primary_scale) : 0.0f;
    self->m_pots[i].syncPhysicalValue(pots[i]);
  }
}

// Draw Euclidean mode UI (length/fill, chaos)
void EuclideanModeStrategy::drawModeUI(NtGridsAlgorithm *self, int y_start, _NT_textSize text_size, int line_spacing, char *buffer)
{
  if (!self || !self->m_platform_adapter)
    return;
  const int white = 15;
  const int gray = 8;
  bool editing_length = m_euclidean_controls_length;
  int length_val_color = editing_length ? white : gray;
  int fill_val_color = editing_length ? gray : white;
  int euclid_y = y_start;
  int chaos_y = euclid_y + line_spacing;
  int x_pos = 30;
  const int sep_width = 5;
  const int val_width = 20;
  const int label_width = 20;
  const int track_gap = 15;
  for (int i = 0; i < 3; ++i)
  {
    ParameterIndex length_param = (ParameterIndex)(kParamEuclideanLength1 + i);
    ParameterIndex fill_param = (ParameterIndex)(kParamEuclideanFill1 + i);
    char label_buf[4];
    label_buf[0] = editing_length ? 'L' : 'F';
    label_buf[1] = '1' + i;
    label_buf[2] = ':';
    label_buf[3] = '\0';
    self->m_platform_adapter->drawText(x_pos, euclid_y, label_buf, gray, kNT_textLeft, text_size);
    x_pos += label_width;
    self->m_platform_adapter->intToString(buffer, self->v[length_param]);
    self->m_platform_adapter->drawText(x_pos, euclid_y, buffer, length_val_color, kNT_textLeft, text_size);
    x_pos += val_width;
    self->m_platform_adapter->drawText(x_pos, euclid_y, ":", gray, kNT_textLeft, text_size);
    x_pos += sep_width;
    int16_t length_param_value = self->v[length_param];
    int16_t fill_param_value_raw = self->v[fill_param];
    uint16_t actual_hits_from_lut = 0;
    if (length_param_value > 0 && length_param_value <= 32)
    {
      uint8_t density_for_lut = static_cast<uint8_t>(fill_param_value_raw >> 3);
      if (density_for_lut > 31)
        density_for_lut = 31;
      uint16_t address = (static_cast<uint16_t>(length_param_value - 1) * 32) + density_for_lut;
      if (address >= nt_grids_port::LUT_RES_EUCLIDEAN_SIZE)
        address = nt_grids_port::LUT_RES_EUCLIDEAN_SIZE - 1;
      actual_hits_from_lut = nt_grids_port::lut_res_euclidean[address];
    }
    self->m_platform_adapter->intToString(buffer, actual_hits_from_lut);
    self->m_platform_adapter->drawText(x_pos, euclid_y, buffer, fill_val_color, kNT_textLeft, text_size);
    x_pos += val_width + track_gap;
  }
  // Chaos Amount
  self->m_platform_adapter->drawText(200, chaos_y, "Chaos:", gray, kNT_textLeft, text_size);
  if (self->v[kParamChaosEnable])
  {
    self->m_platform_adapter->intToString(buffer, self->v[kParamChaosAmount]);
    self->m_platform_adapter->drawText(255, chaos_y, buffer, white, kNT_textRight, text_size);
  }
  else
  {
    self->m_platform_adapter->drawText(255, chaos_y, "Off", gray, kNT_textRight, text_size);
  }
}

// Called when Euclidean mode is activated
void EuclideanModeStrategy::onModeActivated(NtGridsAlgorithm *self)
{
#ifdef TESTING_BUILD
  MockNtPlatformAdapter *mock_adapter = dynamic_cast<MockNtPlatformAdapter *>(self->m_platform_adapter);
  if (self && mock_adapter)
  {
    mock_adapter->logMessage("EuclideanModeStrategy::onModeActivated CALLED (minimal)");
  }
#endif
}

// Called when Euclidean mode is deactivated
void EuclideanModeStrategy::onModeDeactivated(NtGridsAlgorithm * /*self*/)
{
  // No special state to reset for Euclidean mode
}

// Map pots to Euclidean mode parameters (Fill/Length, toggling with m_euclidean_controls_length)
void EuclideanModeStrategy::determinePotConfig(NtGridsAlgorithm *self, int pot_index, ParameterIndex &primary_idx, ParameterIndex &alternate_idx, bool &has_alternate, float &primary_scale, float &alternate_scale)
{
  bool is_primary_length = m_euclidean_controls_length; // Use member variable

  primary_idx = is_primary_length ? (ParameterIndex)(kParamEuclideanLength1 + pot_index) : (ParameterIndex)(kParamEuclideanFill1 + pot_index);
  primary_scale = is_primary_length ? 32.0f : 255.0f;
  alternate_idx = is_primary_length ? (ParameterIndex)(kParamEuclideanFill1 + pot_index) : (ParameterIndex)(kParamEuclideanLength1 + pot_index);
  alternate_scale = is_primary_length ? 255.0f : 32.0f;
  has_alternate = false; // In Euclidean mode, pot buttons are for length/fill toggle, not alternate params on the pot itself.
}

// Return mode name
const char *EuclideanModeStrategy::getModeName() const
{
  return "Euclidean";
}