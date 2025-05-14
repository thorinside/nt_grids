#ifndef MOCK_NT_PLATFORM_ADAPTER_H
#define MOCK_NT_PLATFORM_ADAPTER_H

#include "nt_platform_adapter.h"
#include <string>
#include <vector>
#include <map>

// Forward declare to avoid including s_parameters directly in the mock header
// We'll handle _NT_parameter provision through a map or similar.
// struct _NT_parameter; // Already in distingnt/api.h via nt_platform_adapter.h

class MockNtPlatformAdapter : public INtPlatformAdapter
{
public:
  ~MockNtPlatformAdapter() override = default;

  // --- Observable state / Call recording ---
  mutable uint32_t last_alg_idx_param_set;
  mutable uint32_t last_param_idx_offset_param_set;
  mutable int32_t last_value_param_set;
  mutable int call_count_setParameterFromUi = 0;

  mutable int16_t last_draw_text_x;
  mutable int16_t last_draw_text_y;
  mutable std::string last_draw_text_str;
  mutable uint8_t last_draw_text_c;
  mutable _NT_textAlignment last_draw_text_align;
  mutable _NT_textSize last_draw_text_size;
  mutable int call_count_drawText = 0;

  mutable std::vector<std::string> drawn_texts_history;

  mutable std::string last_int_to_string_input_buffer_val_on_call; // Not easily testable for output
  mutable int32_t last_int_to_string_value;
  mutable int call_count_intToString = 0;

  // For direct logging from tests or algorithm if needed
  std::vector<std::string> logged_messages_history;
  void logMessage(const char *text)
  {
    logged_messages_history.push_back(std::string(text));
  }

  // --- Configurable behavior ---
  uint32_t mock_alg_idx = 0;
  uint32_t mock_param_offset = 0;
  float mock_sample_rate = 48000.0f;
  std::map<ParameterIndex, _NT_parameter> mock_param_definitions;
  uint16_t mock_pot_button_mask_L = (1 << 0); // kNT_potButtonL typically
  uint16_t mock_pot_button_mask_C = (1 << 1); // kNT_potButtonC
  uint16_t mock_pot_button_mask_R = (1 << 2); // kNT_potButtonR

  // --- Method implementations ---
  void setParameterFromUi(uint32_t alg_idx, uint32_t param_idx_with_offset, int32_t value) override
  {
    call_count_setParameterFromUi++;
    last_alg_idx_param_set = alg_idx;
    last_param_idx_offset_param_set = param_idx_with_offset;
    last_value_param_set = value;
    // MESSAGE("MockNtPlatformAdapter::setParameterFromUi CALLED. alg_idx: ", alg_idx, ", param_idx_with_offset: ", param_idx_with_offset, ", value: ", value); // Diagnostic
  }

  void drawText(int16_t x, int16_t y, const char *str, uint8_t c, _NT_textAlignment align, _NT_textSize size) override
  {
    last_draw_text_x = x;
    last_draw_text_y = y;
    last_draw_text_str = str ? str : "";
    last_draw_text_c = c;
    last_draw_text_align = align;
    last_draw_text_size = size;
    drawn_texts_history.push_back(last_draw_text_str);
    call_count_drawText++;
  }

  void intToString(char *buffer, int32_t value) override
  {
    // last_int_to_string_input_buffer_val_on_call = buffer ? std::string(buffer) : ""; // Risky if buffer isn't init
    last_int_to_string_value = value;
    // Simulate actual intToString for basic cases if needed by tests, or keep it simple
    // For now, just record and use a standard sprintf for basic testability if buffer is provided.
    if (buffer)
    {
      snprintf(buffer, 32, "%d", value); // Max 32 chars, typical for small displays
    }
    call_count_intToString++;
  }

  uint32_t getAlgorithmIndex(_NT_algorithm * /*self_base*/) override
  {
    return mock_alg_idx;
  }

  uint32_t getParameterOffset() override
  {
    return mock_param_offset;
  }

  float getSampleRate() override
  {
    return mock_sample_rate;
  }

  const _NT_parameter *getParameterDefinition(ParameterIndex p_idx) override
  {
    auto it = mock_param_definitions.find(p_idx);
    if (it != mock_param_definitions.end())
    {
      return &it->second;
    }
    // Optionally, return a default constructed _NT_parameter or nullptr
    // For tests, it's better if this fails clearly if a param isn't mocked.
    // However, to prevent crashes, returning a static default might be safer.
    static _NT_parameter default_param = {};
    // Consider logging an error or throwing if a param def is requested but not mocked.
    // For now, returning a default to allow tests to focus on other aspects.
    // REQUIRE(mock_param_definitions.count(p_idx) > 0); // Alternative: make tests fail here
    return &default_param; // Return a pointer to a valid (but empty) static object
  }

  uint16_t getPotButtonMask(int pot_index) override
  {
    if (pot_index == 0)
      return mock_pot_button_mask_L; // Pot L
    if (pot_index == 1)
      return mock_pot_button_mask_C; // Pot C
    if (pot_index == 2)
      return mock_pot_button_mask_R; // Pot R
    // Add cases for encoder buttons if adapter distinguishes them
    // Or if kNT_encoderButtonL/R are distinct values handled elsewhere
    return 0; // Default / error
  }

  // Helper to prime parameter definitions for tests
  void primeParameterDefinition(ParameterIndex p_idx, const _NT_parameter &def)
  {
    mock_param_definitions[p_idx] = def;
  }

  void resetMockData()
  {
    call_count_setParameterFromUi = 0;
    call_count_drawText = 0;
    drawn_texts_history.clear();
    call_count_intToString = 0;
    // Reset last_... variables to default/neutral states if necessary
    last_draw_text_str = "";

    // mock_param_definitions.clear(); // Decide if this should be cleared on reset
  }
};

#endif // MOCK_NT_PLATFORM_ADAPTER_H