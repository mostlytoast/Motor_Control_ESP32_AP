#include "OptionsMenu.h"

OptionsMenu::OptionsMenu(lv_obj_t* parent,
                         std::function<void()> sendStateToReceiverCallback_) {
  sendStateToReceiverCallback = sendStateToReceiverCallback_;

  createMenu(parent);
}

// ============================================================
// CREATE UI
// ============================================================

void OptionsMenu::createMenu(lv_obj_t* parent) {
  root_ = lv_obj_create(parent);

  if (!root_) {
    Serial.println("FAILED creating root");
    return;
  }

  Serial.println("root created");

  lv_obj_remove_style_all(root_);

  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_align(root_, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_set_style_bg_color(root_, lv_color_hex(colorConfig.backgroundColor),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
  // Set parent background.
  lv_obj_set_scrollable(root_, false);

  grid_cont_ = lv_obj_create(root_);

  Serial.printf("[DISPLAY] Grid created: %p\n", grid_cont_);

  lv_obj_set_width(grid_cont_, lv_pct(100));
  lv_obj_set_height(grid_cont_, lv_pct(100));

  /* Center the container on the screen */
  lv_obj_center(grid_cont_);
  lv_obj_set_style_bg_opa(grid_cont_, LV_OPA_TRANSP, LV_PART_MAIN);

  // lv_obj_set_style_bg_color(
  //     grid_cont_, lv_color_hex(colorConfig.backgroundColor), LV_PART_MAIN);
  static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2),
                              LV_GRID_TEMPLATE_LAST};

  /* Define 2 rows: equal height */
  static int32_t row_dsc[] = {
      LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
      LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(5), LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(grid_cont_, col_dsc, row_dsc);
  lv_obj_set_layout(grid_cont_, LV_LAYOUT_GRID);

  // Initialize motor controls.
  // Keep the original root_ argument.
  lv_obj_t* keyboard =
      sharedDisplay.createKeyboard(root_, "", 0, 0, lv_pct(100), lv_pct(60));
  lv_obj_set_hidden(keyboard, true);
  lv_obj_t* hide_button = sharedDisplay.createActionButton(
      grid_cont_, "Hide", 215, 27, 150, 25,
      [this]() { toggle(); },
      lv_color_hex(colorConfig.menuHeaderColor));
  lv_obj_set_grid_cell(hide_button, LV_GRID_ALIGN_STRETCH, 1, 0,
                       LV_GRID_ALIGN_STRETCH, 0, 1);
  //  ============================= MIN PWM =============================
  lv_obj_t* min_pwm_label = sharedDisplay.createLabel(
      grid_cont_, lv_color_hex(colorConfig.textColor), 0, 2, LV_PART_MAIN,
      LV_ALIGN_CENTER, app.getMinPwmLabelLambda);
  lv_obj_set_grid_cell(min_pwm_label, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);

  lv_obj_t* min_pwm_slider = sharedDisplay.createActionSlider(
      grid_cont_, app.setMinPwmLambda, app.getMinPwmLambda,
      static_cast<float>(MIN_PWM), static_cast<float>(MAX_PWM),
      lv_color_hex(colorConfig.infoColor));
  lv_obj_set_grid_cell(min_pwm_slider, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 1, 1);
  //  ============================= LOW AUDIO =============================

  lv_obj_t* low_audio_offset_label = sharedDisplay.createLabel(
      grid_cont_, lv_color_hex(colorConfig.textColor), 0, 2, LV_PART_MAIN,
      LV_ALIGN_CENTER, app.getLowAudioOffsetLabelLambda);
  lv_obj_set_grid_cell(low_audio_offset_label, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 2, 1);

  lv_obj_t* low_audio_offset_slider = sharedDisplay.createActionSlider(
      grid_cont_, app.setLowAudioOffsetLambda, app.getLowAudioOffsetLambda, 0.0,
      5.0, lv_color_hex(colorConfig.infoColor));
  lv_obj_set_grid_cell(low_audio_offset_slider, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 2, 1);

  //  ============================= HIGH AUDIO =============================

  lv_obj_t* high_audio_offset_label = sharedDisplay.createLabel(
      grid_cont_, lv_color_hex(colorConfig.textColor), 0, 2, LV_PART_MAIN,
      LV_ALIGN_CENTER, app.getHighAudioOffsetLabelLambda);
  lv_obj_set_grid_cell(high_audio_offset_label, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);

  lv_obj_t* high_audio_offset_slider = sharedDisplay.createActionSlider(
      grid_cont_, app.setHighAudioOffsetLambda, app.getHighAudioOffsetLambda,
      0.0, 5.0, lv_color_hex(colorConfig.infoColor));
  lv_obj_set_grid_cell(high_audio_offset_slider, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 3, 1);
  //  ============================= AMBIANCE =============================

  lv_obj_t* ambiance_sound_volume_slider = sharedDisplay.createLabel(
      grid_cont_, lv_color_hex(colorConfig.textColor), 0, 2, LV_PART_MAIN,
      LV_ALIGN_CENTER, app.getAmbianceSoundVolumeLabelLambda);
  lv_obj_set_grid_cell(ambiance_sound_volume_slider, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 4, 1);

  lv_obj_t* ambiance_sound_volume_label = sharedDisplay.createActionSlider(
      grid_cont_, app.setAmbianceSoundVolumeLambda,
      app.getAmbianceSoundVolumeLambda, 0.0, 11.0,
      lv_color_hex(colorConfig.infoColor));
  lv_obj_set_grid_cell(ambiance_sound_volume_label, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 4, 1);

  //  ============================= MOVEMENT =============================

  lv_obj_t* movement_sound_volume_slider = sharedDisplay.createLabel(
      grid_cont_, lv_color_hex(colorConfig.textColor), 0, 2, LV_PART_MAIN,
      LV_ALIGN_CENTER, app.getMovementSoundVolumeLabelLambda);
  lv_obj_set_grid_cell(movement_sound_volume_slider, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_STRETCH, 5, 1);

  lv_obj_t* movement_sound_volume_label = sharedDisplay.createActionSlider(
      grid_cont_, app.setMovementSoundVolumeLambda,
      app.getMovementSoundVolumeLambda, 0.0, 11.0,
      lv_color_hex(colorConfig.infoColor));
  lv_obj_set_grid_cell(movement_sound_volume_label, LV_GRID_ALIGN_STRETCH, 1, 1,
                       LV_GRID_ALIGN_STRETCH, 5, 1);
  lv_obj_set_hidden(root_, true);
}

void OptionsMenu::refreshAndSend() {
  refresh();
  if (sendStateToReceiverCallback != nullptr) {
    sendStateToReceiverCallback();
  }
}

// ============================================================
// REFRESH
// ============================================================

void OptionsMenu::refresh() {
  if (!root_ || lv_obj_is_hidden(root_)) {
    return;
  }
  sharedDisplay.updateStates();
  if (root_) {
    lv_obj_invalidate(root_);
  }
}

void OptionsMenu::toggle() {
  // Serial.print("call toggle");

  if (lv_obj_is_hidden(root_)) {
    // lv_obj_move_to_index(root_, 0);

    lv_obj_set_hidden(root_, false);
    lv_obj_move_foreground(root_);
    // Serial.print("show");
  } else {
    lv_obj_set_hidden(root_, true);
    // lv_obj_move_to_index(root_, 2);

    // Serial.print("hide");
  }
}