#include "ui.h"

#include <lvgl.h>

#include <algorithm>
#include <cstring>

namespace {

lv_color_t color(uint32_t hex) {
  return lv_color_hex(hex);
}

constexpr uint32_t kGreen = 0xA8FF4F;
constexpr uint32_t kWhite = 0xF4F7ED;
constexpr uint32_t kMuted = 0x718079;
constexpr uint32_t kPanel = 0x0B1210;
constexpr uint32_t kTrack = 0x34483D;
constexpr uint32_t kDivider = 0x1E2A25;
constexpr uint32_t kAmber = 0xFFB02E;
constexpr uint32_t kRed = 0xFF5F57;
constexpr uint32_t kBlue = 0x55E6D2;
constexpr uint32_t kViolet = 0xA5B4FC;

// Adjustable product thresholds, expressed as used percentage.
constexpr int16_t kQuotaWarningPercent = 80;
constexpr int16_t kQuotaCriticalPercent = 95;

constexpr int kPageWidth = 466;
constexpr int kPageHeight = 352;
constexpr int kPageTop = 72;
constexpr size_t kPageCount = 4;

UiActionHandler action_handler = nullptr;

lv_obj_t* tileview = nullptr;
lv_obj_t* dim_overlay = nullptr;
lv_obj_t* pages[kPageCount] = {};
lv_obj_t* page_dots[kPageCount] = {};
lv_obj_t* page_title_label = nullptr;
lv_obj_t* connection_label = nullptr;
lv_obj_t* battery_label = nullptr;
lv_obj_t* truncation_label = nullptr;

lv_obj_t* success_arc = nullptr;
lv_obj_t* success_value_label = nullptr;
lv_obj_t* calls_value_label = nullptr;
lv_obj_t* errors_value_label = nullptr;
lv_obj_t* quota_label = nullptr;

lv_obj_t* usage_content = nullptr;
lv_obj_t* usage_provider_label = nullptr;
lv_obj_t* usage_window_label = nullptr;
lv_obj_t* usage_gauge = nullptr;
lv_obj_t* usage_value_label = nullptr;
lv_obj_t* usage_caption_label = nullptr;
lv_obj_t* usage_remaining_label = nullptr;
lv_obj_t* usage_divider = nullptr;
lv_obj_t* usage_reset_label = nullptr;
lv_obj_t* usage_reset_time_label = nullptr;
lv_obj_t* usage_secondary_labels[3] = {};
lv_obj_t* usage_empty_label = nullptr;
lv_obj_t* provider_dots[kMaxProviders] = {};

lv_obj_t* session_count_label = nullptr;
lv_obj_t* session_tokens_label = nullptr;
lv_obj_t* session_cost_label = nullptr;
lv_obj_t* top_model_label = nullptr;
lv_obj_t* top_model_calls_label = nullptr;

lv_obj_t* cache_arc = nullptr;
lv_obj_t* cache_value_label = nullptr;
lv_obj_t* context_input_label = nullptr;
lv_obj_t* context_output_label = nullptr;
lv_obj_t* context_cache_label = nullptr;
lv_obj_t* context_reasoning_label = nullptr;
lv_obj_t* context_total_label = nullptr;
lv_obj_t* mix_segments[4] = {};

lv_obj_t* quick_actions_overlay = nullptr;
lv_obj_t* pairing_overlay = nullptr;
lv_obj_t* pairing_code_label = nullptr;
lv_obj_t* toast = nullptr;
lv_timer_t* toast_timer = nullptr;

AppState current_state;
uint32_t current_elapsed = 0;
bool has_state = false;
char selected_provider_id[sizeof(ProviderState::id)] = "";
size_t selected_provider_index = 0;

void styleLabel(lv_obj_t* label, const lv_font_t* font, uint32_t text_color) {
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, color(text_color), LV_PART_MAIN);
  lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
}

lv_obj_t* createLabel(lv_obj_t* parent, const char* text,
                      const lv_font_t* font, uint32_t text_color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  styleLabel(label, font, text_color);
  return label;
}

lv_obj_t* createCenteredLabel(lv_obj_t* parent, int y, const char* text,
                              const lv_font_t* font, uint32_t text_color,
                              int width = 360) {
  lv_obj_t* label = createLabel(parent, text, font, text_color);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

uint32_t quotaColor(int16_t used_percent) {
  if (used_percent >= kQuotaCriticalPercent) {
    return kRed;
  }
  if (used_percent >= kQuotaWarningPercent) {
    return kAmber;
  }
  return kGreen;
}

void hideScrollbars(lv_obj_t* object) {
  lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_OFF);
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(object); ++i) {
    hideScrollbars(lv_obj_get_child(object, i));
  }
}

void setArcColor(lv_obj_t* arc, uint32_t arc_color) {
  lv_obj_set_style_arc_color(arc, color(arc_color), LV_PART_INDICATOR);
}

void createRing(lv_obj_t* parent, lv_obj_t** arc, lv_obj_t** value,
                const char* caption, int x, int y, int size) {
  *arc = lv_arc_create(parent);
  lv_obj_set_pos(*arc, x, y);
  lv_obj_set_size(*arc, size, size);
  lv_arc_set_rotation(*arc, 270);
  lv_arc_set_bg_angles(*arc, 0, 360);
  lv_arc_set_range(*arc, 0, 1000);
  lv_arc_set_value(*arc, 0);
  lv_obj_remove_style(*arc, nullptr, LV_PART_KNOB);
  lv_obj_set_style_arc_width(*arc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_color(*arc, color(kTrack), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(*arc, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_width(*arc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(*arc, color(kGreen), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(*arc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_clear_flag(*arc, LV_OBJ_FLAG_CLICKABLE);

  *value = createLabel(parent, "--.-", &lv_font_montserrat_48, kWhite);
  lv_obj_align_to(*value, *arc, LV_ALIGN_CENTER, 0, -18);
  lv_obj_t* unit_label =
      createLabel(parent, "%", &lv_font_montserrat_18, kGreen);
  lv_obj_align_to(unit_label, *arc, LV_ALIGN_CENTER, 0, 25);
  lv_obj_t* caption_label =
      createLabel(parent, caption, &lv_font_montserrat_12, kMuted);
  lv_obj_align_to(caption_label, *arc, LV_ALIGN_CENTER, 0, 48);
}

void createUsageGauge(lv_obj_t* parent) {
  usage_gauge = lv_arc_create(parent);
  lv_obj_set_pos(usage_gauge, 133, 48);
  lv_obj_set_size(usage_gauge, 200, 200);
  lv_arc_set_rotation(usage_gauge, 0);
  lv_arc_set_bg_angles(usage_gauge, 135, 405);
  lv_arc_set_range(usage_gauge, 0, 100);
  lv_arc_set_value(usage_gauge, 0);
  lv_obj_remove_style(usage_gauge, nullptr, LV_PART_KNOB);
  lv_obj_set_style_arc_width(usage_gauge, 14, LV_PART_MAIN);
  lv_obj_set_style_arc_color(usage_gauge, color(kTrack), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(usage_gauge, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_width(usage_gauge, 14, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(usage_gauge, color(kGreen), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(usage_gauge, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_clear_flag(usage_gauge, LV_OBJ_FLAG_CLICKABLE);

  // Fixed-width centered labels remain centered as digits and fonts change.
  usage_value_label =
      createCenteredLabel(parent, 104, "--", &lv_font_montserrat_48,
                          kWhite, 186);
  usage_caption_label =
      createCenteredLabel(parent, 168, "USED", &lv_font_montserrat_12,
                          kMuted, 180);
}

size_t activePageIndex() {
  if (tileview == nullptr) {
    return 0;
  }
  lv_obj_t* active = lv_tileview_get_tile_act(tileview);
  for (size_t index = 0; index < kPageCount; ++index) {
    if (pages[index] == active) {
      return index;
    }
  }
  return 0;
}

const char* pageTitle(size_t index) {
  switch (index) {
    case 0:
      return "OVERVIEW  /  TODAY";
    case 1:
      return "USAGE  /  QUOTA";
    case 2:
      return "SESSIONS  /  30 DAYS";
    case 3:
      return "CONTEXT  /  30 DAYS";
    default:
      return "";
  }
}

void refreshPageDots() {
  const size_t active = activePageIndex();
  for (size_t index = 0; index < kPageCount; ++index) {
    const bool selected = index == active;
    lv_obj_set_style_bg_color(
        page_dots[index], color(selected ? kGreen : kMuted), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        page_dots[index], selected ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
  }
  if (page_title_label != nullptr) {
    lv_label_set_text(page_title_label, pageTitle(active));
  }
}

void refreshProviderDots() {
  const size_t active = activePageIndex();
  const uint8_t count = current_state.provider_count;
  const bool show = active == 1 && count > 1;
  for (size_t index = 0; index < kMaxProviders; ++index) {
    lv_obj_t* dot = provider_dots[index];
    if (!show || index >= count) {
      lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    const int total = static_cast<int>(count) * 14 - 7;
    const int y = 233 - total / 2 + static_cast<int>(index) * 14;
    lv_obj_set_pos(dot, 432, y);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
    const bool selected = index == selected_provider_index;
    lv_obj_set_style_bg_color(
        dot, color(selected ? kGreen : kMuted), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        dot, selected ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
  }
}

void updateUsagePage();

void displayPageChanged(lv_event_t*) {
  refreshPageDots();
  refreshProviderDots();
  if (has_state) {
    updateUsagePage();
  }
}

void dispatchAction(lv_event_t* event) {
  if (action_handler == nullptr) {
    return;
  }
  const auto raw = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  action_handler(static_cast<UiAction>(raw));
}

void closeOverlay(lv_event_t*) {
  lv_obj_add_flag(quick_actions_overlay, LV_OBJ_FLAG_HIDDEN);
}

void closePairingOverlay(lv_event_t*) {
  uiHidePairingCode();
}

void dispatchOverlayAction(lv_event_t* event) {
  lv_obj_add_flag(quick_actions_overlay, LV_OBJ_FLAG_HIDDEN);
  dispatchAction(event);
}

void createActionButton(lv_obj_t* parent, const char* text, int y,
                        UiAction action) {
  lv_obj_t* button = lv_btn_create(parent);
  lv_obj_set_pos(button, 73, y);
  lv_obj_set_size(button, 320, 54);
  lv_obj_set_style_radius(button, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, color(kPanel), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(button, color(kGreen), LV_PART_MAIN);
  lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(button, dispatchOverlayAction, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(action)));

  lv_obj_t* label = createLabel(button, text, &lv_font_montserrat_18, kWhite);
  lv_obj_center(label);
}

void hideToast(lv_timer_t*) {
  lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
  toast_timer = nullptr;
}

String formatCost(uint32_t micros) {
  return "$" + String(static_cast<double>(micros) / 1000000.0, 3);
}

void createOverviewPage(lv_obj_t* page) {
  createRing(page, &success_arc, &success_value_label, "SUCCESS", (kPageWidth - 190) / 2, 10, 190);

  calls_value_label =
      createCenteredLabel(page, 225, "--", &lv_font_montserrat_32, kWhite,
                          130);
  lv_obj_align(calls_value_label, LV_ALIGN_TOP_LEFT, 66, 225);
  lv_obj_t* calls_caption =
      createCenteredLabel(page, 264, "CALLS", &lv_font_montserrat_12, kMuted,
                          130);
  lv_obj_align(calls_caption, LV_ALIGN_TOP_LEFT, 66, 264);

  errors_value_label =
      createCenteredLabel(page, 225, "--", &lv_font_montserrat_32, kWhite,
                          130);
  lv_obj_align(errors_value_label, LV_ALIGN_TOP_RIGHT, -66, 225);
  lv_obj_t* errors_caption =
      createCenteredLabel(page, 264, "ERRORS", &lv_font_montserrat_12,
                          kMuted, 130);
  lv_obj_align(errors_caption, LV_ALIGN_TOP_RIGHT, -66, 264);

  lv_obj_t* divider = lv_obj_create(page);
  lv_obj_remove_style_all(divider);
  lv_obj_set_pos(divider, 232, 228);
  lv_obj_set_size(divider, 1, 56);
  lv_obj_set_style_bg_color(divider, color(kDivider), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);

  createCenteredLabel(page, 288, "LOWEST QUOTA", &lv_font_montserrat_12,
                      kMuted, 280);
  quota_label =
      createCenteredLabel(page, 306, "--", &lv_font_montserrat_26, kWhite,
                          280);
  truncation_label =
      createCenteredLabel(page, 334, "", &lv_font_montserrat_12, kAmber, 280);
  lv_obj_add_flag(truncation_label, LV_OBJ_FLAG_HIDDEN);
}

void createUsagePage(lv_obj_t* page) {
  usage_content = lv_obj_create(page);
  lv_obj_remove_style_all(usage_content);
  lv_obj_set_pos(usage_content, 0, 0);
  lv_obj_set_size(usage_content, kPageWidth, kPageHeight);
  lv_obj_clear_flag(usage_content, LV_OBJ_FLAG_SCROLLABLE);

  usage_provider_label =
      createCenteredLabel(usage_content, 0, "-", &lv_font_montserrat_18,
                          kGreen, 340);
  usage_window_label =
      createCenteredLabel(usage_content, 25, "-", &lv_font_montserrat_12,
                          kMuted, 340);
  lv_label_set_long_mode(usage_provider_label, LV_LABEL_LONG_DOT);
  lv_label_set_long_mode(usage_window_label, LV_LABEL_LONG_DOT);

  createUsageGauge(usage_content);

  usage_remaining_label =
      createCenteredLabel(usage_content, 232, "-- REMAINING",
                          &lv_font_montserrat_18, kWhite, 300);

  usage_divider = lv_obj_create(usage_content);
  lv_obj_remove_style_all(usage_divider);
  lv_obj_set_pos(usage_divider, 103, 260);
  lv_obj_set_size(usage_divider, 260, 1);
  lv_obj_set_style_bg_color(usage_divider, color(kDivider), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(usage_divider, LV_OPA_COVER, LV_PART_MAIN);

  usage_reset_label =
      createCenteredLabel(usage_content, 268, "RESETS IN --",
                          &lv_font_montserrat_14, kMuted, 320);
  usage_reset_time_label =
      createCenteredLabel(usage_content, 289, "--/-- --:--",
                          &lv_font_montserrat_12, kMuted, 320);

  for (size_t index = 0; index < 3; ++index) {
    usage_secondary_labels[index] =
        createCenteredLabel(usage_content, 304 + static_cast<int>(index) * 16,
                            "", &lv_font_montserrat_12, kMuted, 250);
    lv_label_set_long_mode(usage_secondary_labels[index],
                           LV_LABEL_LONG_DOT);
    lv_obj_add_flag(usage_secondary_labels[index], LV_OBJ_FLAG_HIDDEN);
  }

  usage_empty_label =
      createCenteredLabel(usage_content, 120, "NO QUOTA DATA",
                          &lv_font_montserrat_18, kMuted, 300);
  lv_obj_add_flag(usage_empty_label, LV_OBJ_FLAG_HIDDEN);
}

void createSessionsPage(lv_obj_t* page) {
  session_count_label =
      createCenteredLabel(page, 8, "--", &lv_font_montserrat_48, kWhite, 300);
  createCenteredLabel(page, 70, "SESSIONS", &lv_font_montserrat_12, kMuted,
                      300);

  lv_obj_t* divider = lv_obj_create(page);
  lv_obj_remove_style_all(divider);
  lv_obj_set_pos(divider, 93, 94);
  lv_obj_set_size(divider, 280, 1);
  lv_obj_set_style_bg_color(divider, color(kDivider), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);

  session_tokens_label = createLabel(page, "--", &lv_font_montserrat_32,
                                     kWhite);
  lv_obj_set_width(session_tokens_label, 160);
  lv_obj_set_style_text_align(session_tokens_label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(session_tokens_label, LV_ALIGN_TOP_LEFT, 60, 108);
  lv_obj_t* tokens_caption =
      createCenteredLabel(page, 146, "TOTAL TOKENS",
                          &lv_font_montserrat_12, kMuted, 160);
  lv_obj_align(tokens_caption, LV_ALIGN_TOP_LEFT, 60, 146);

  session_cost_label = createLabel(page, "$--", &lv_font_montserrat_32,
                                   kWhite);
  lv_obj_set_width(session_cost_label, 160);
  lv_obj_set_style_text_align(session_cost_label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(session_cost_label, LV_ALIGN_TOP_RIGHT, -60, 108);
  lv_obj_t* cost_caption =
      createCenteredLabel(page, 146, "EST. COST", &lv_font_montserrat_12,
                          kMuted, 160);
  lv_obj_align(cost_caption, LV_ALIGN_TOP_RIGHT, -60, 146);

  lv_obj_t* divider2 = lv_obj_create(page);
  lv_obj_remove_style_all(divider2);
  lv_obj_set_pos(divider2, 93, 172);
  lv_obj_set_size(divider2, 280, 1);
  lv_obj_set_style_bg_color(divider2, color(kDivider), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(divider2, LV_OPA_COVER, LV_PART_MAIN);

  createCenteredLabel(page, 182, "TOP MODEL", &lv_font_montserrat_12, kMuted,
                      320);
  top_model_label =
      createCenteredLabel(page, 202, "-", &lv_font_montserrat_26, kWhite, 320);
  lv_label_set_long_mode(top_model_label, LV_LABEL_LONG_DOT);
  top_model_calls_label =
      createCenteredLabel(page, 240, "-- CALLS", &lv_font_montserrat_14,
                          kGreen, 320);
}

void createContextPage(lv_obj_t* page) {
  createRing(page, &cache_arc, &cache_value_label, "CACHE HIT", 143, 6, 180);

  lv_obj_t* mix_title =
      createCenteredLabel(page, 196, "TOKEN MIX", &lv_font_montserrat_12,
                          kMuted, 280);
  lv_obj_set_style_text_align(mix_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  constexpr uint32_t colors[] = {kBlue, kGreen, kViolet, kAmber};
  for (size_t index = 0; index < 4; ++index) {
    mix_segments[index] = lv_obj_create(page);
    lv_obj_remove_style_all(mix_segments[index]);
    lv_obj_set_pos(mix_segments[index], 83, 216);
    lv_obj_set_size(mix_segments[index], 0, 8);
    lv_obj_set_style_bg_color(mix_segments[index], color(colors[index]),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mix_segments[index], LV_OPA_COVER, LV_PART_MAIN);
  }

  context_input_label =
      createLabel(page, "INPUT --", &lv_font_montserrat_12, kBlue);
  lv_obj_set_pos(context_input_label, 96, 236);
  context_output_label =
      createLabel(page, "OUTPUT --", &lv_font_montserrat_12, kGreen);
  lv_obj_set_pos(context_output_label, 260, 236);
  context_cache_label =
      createLabel(page, "CACHE --", &lv_font_montserrat_12, kViolet);
  lv_obj_set_pos(context_cache_label, 96, 262);
  context_reasoning_label =
      createLabel(page, "REASON --", &lv_font_montserrat_12, kAmber);
  lv_obj_set_pos(context_reasoning_label, 260, 262);

  context_total_label =
      createCenteredLabel(page, 292, "TOTAL -- TOKENS",
                          &lv_font_montserrat_14, kMuted, 300);
}

void selectProvider() {
  if (current_state.provider_count == 0) {
    selected_provider_index = 0;
    selected_provider_id[0] = '\0';
    return;
  }

  size_t found = current_state.provider_count;
  if (selected_provider_id[0] != '\0') {
    for (size_t index = 0; index < current_state.provider_count; ++index) {
      if (strcmp(current_state.providers[index].id,
                 selected_provider_id) == 0) {
        found = index;
        break;
      }
    }
  }
  if (found >= current_state.provider_count) {
    found = 0;
  }
  selected_provider_index = found;
  strlcpy(selected_provider_id, current_state.providers[found].id,
          sizeof(selected_provider_id));
}

void updateOverview() {
  if (!current_state.data_valid) {
    lv_arc_set_value(success_arc, 0);
    lv_label_set_text(success_value_label, "--.-");
    lv_label_set_text(calls_value_label, "--");
    lv_label_set_text(errors_value_label, "--");
    lv_label_set_text(quota_label, "--");
    lv_obj_add_flag(truncation_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_arc_set_value(success_arc,
                   std::min<uint16_t>(current_state.success_permille, 1000));
  lv_label_set_text_fmt(
      success_value_label, "%u.%u",
      static_cast<unsigned int>(current_state.success_permille / 10),
      static_cast<unsigned int>(current_state.success_permille % 10));
  lv_obj_align_to(success_value_label, success_arc, LV_ALIGN_CENTER, 0, -18);
  lv_label_set_text(calls_value_label,
                    formatCompactTokens(current_state.calls_today).c_str());
  lv_label_set_text(
      errors_value_label,
      formatCompactTokens(current_state.errors_today).c_str());
  lv_obj_set_style_text_color(
      errors_value_label,
      color(current_state.errors_today > 0 ? kAmber : kWhite),
      LV_PART_MAIN);

  const int16_t lowest = current_state.lowest_remaining_percent;
  if (lowest >= 0) {
    lv_label_set_text_fmt(quota_label, "%d%% LEFT", lowest);
    lv_obj_set_style_text_color(
        quota_label, color(quotaColor(100 - lowest)), LV_PART_MAIN);
  } else {
    lv_label_set_text(quota_label, "--");
    lv_obj_set_style_text_color(quota_label, color(kMuted), LV_PART_MAIN);
  }

  if (current_state.batch_truncated) {
    lv_label_set_text_fmt(truncation_label, "TRUNCATED +%u",
                          static_cast<unsigned int>(
                              current_state.batch_truncated_count));
    lv_obj_clear_flag(truncation_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(truncation_label, LV_OBJ_FLAG_HIDDEN);
  }
}

void hideUsageGauge() {
  lv_obj_add_flag(usage_gauge, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(usage_value_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(usage_caption_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(usage_remaining_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(usage_divider, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(usage_reset_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(usage_reset_time_label, LV_OBJ_FLAG_HIDDEN);
  for (lv_obj_t* label : usage_secondary_labels) {
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
  }
}

void showUsageGauge() {
  lv_obj_clear_flag(usage_gauge, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(usage_value_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(usage_caption_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(usage_remaining_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(usage_divider, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(usage_reset_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(usage_reset_time_label, LV_OBJ_FLAG_HIDDEN);
}

uint32_t windowColor(const QuotaWindowState& window) {
  if (!window.valid) {
    return kMuted;
  }
  if (window.unlimited) {
    return kGreen;
  }
  if (window.used_percent < 0) {
    return kMuted;
  }
  return quotaColor(window.used_percent);
}

String secondaryWindowText(const QuotaWindowState& window,
                           const AppState& state, uint32_t elapsed_seconds) {
  if (!window.valid) {
    return String(window.name) + "  NO DATA";
  }
  if (window.unlimited) {
    return String(window.name) + "  UNLIMITED";
  }

  String text = String(window.name) + "  ";
  if (window.used_percent >= 0) {
    text += String(static_cast<int>(window.used_percent)) + "%";
  } else {
    text += "--";
  }
  if (window.resets_at > 0 && state.time_valid) {
    const int64_t remaining =
        window.resets_at -
        static_cast<int64_t>(state.unix_time + elapsed_seconds);
    if (remaining > 0) {
      text += "  /  " + formatCountdown(remaining);
    }
  }
  return text;
}

void updateUsagePage() {
  const bool usage_active = activePageIndex() == 1;
  if (!usage_active) {
    return;
  }

  if (!current_state.data_valid || current_state.provider_count == 0) {
    lv_label_set_text(usage_provider_label, "-");
    lv_label_set_text(usage_window_label, "");
    hideUsageGauge();
    lv_label_set_text(usage_empty_label, "NO QUOTA DATA");
    lv_obj_clear_flag(usage_empty_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  const ProviderState& provider =
      current_state.providers[selected_provider_index];
  lv_label_set_text(usage_provider_label, provider.name);

  if (provider.window_count == 0) {
    lv_label_set_text(usage_window_label, "");
    hideUsageGauge();
    lv_label_set_text(usage_empty_label, "NO WINDOWS REPORTED");
    lv_obj_clear_flag(usage_empty_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_add_flag(usage_empty_label, LV_OBJ_FLAG_HIDDEN);
  showUsageGauge();

  const QuotaWindowState& primary = provider.windows[0];
  lv_label_set_text(usage_window_label, primary.name);

  if (!primary.valid) {
    lv_arc_set_value(usage_gauge, 0);
    setArcColor(usage_gauge, kMuted);
    lv_obj_set_style_text_font(usage_value_label, &lv_font_montserrat_48,
                               LV_PART_MAIN);
    lv_label_set_text(usage_value_label, "--");
    lv_obj_set_style_text_color(usage_value_label, color(kMuted),
                                LV_PART_MAIN);
    lv_label_set_text(usage_caption_label, "NO DATA");
    lv_label_set_text(usage_remaining_label, "QUOTA UNKNOWN");
    lv_obj_set_style_text_color(usage_remaining_label, color(kMuted),
                                LV_PART_MAIN);
  } else if (primary.unlimited) {
    lv_arc_set_value(usage_gauge, 0);
    setArcColor(usage_gauge, kGreen);
    lv_obj_set_style_text_font(usage_value_label, &lv_font_montserrat_26,
                               LV_PART_MAIN);
    lv_label_set_text(usage_value_label, "UNLIMITED");
    lv_obj_set_style_text_color(usage_value_label, color(kGreen),
                                LV_PART_MAIN);
    lv_label_set_text(usage_caption_label, "NO LIMIT");
    lv_label_set_text(usage_remaining_label, "NO LIMIT");
    lv_obj_set_style_text_color(usage_remaining_label, color(kGreen),
                                LV_PART_MAIN);
  } else {
    const int16_t used =
        primary.used_percent >= 0 ? primary.used_percent : 0;
    const uint32_t accent = quotaColor(used);
    lv_arc_set_value(usage_gauge, used);
    setArcColor(usage_gauge, accent);
    lv_obj_set_style_text_font(usage_value_label, &lv_font_montserrat_48,
                               LV_PART_MAIN);
    if (primary.used_percent >= 0) {
      lv_label_set_text_fmt(usage_value_label, "%d%%",
                            static_cast<int>(primary.used_percent));
    } else {
      lv_label_set_text(usage_value_label, "--");
    }
    lv_obj_set_style_text_color(usage_value_label, color(accent),
                                LV_PART_MAIN);
    lv_label_set_text(usage_caption_label, "USED");
    if (primary.remaining_percent >= 0) {
      lv_label_set_text_fmt(usage_remaining_label, "%d%% REMAINING",
                            static_cast<int>(primary.remaining_percent));
    } else {
      lv_label_set_text(usage_remaining_label, "-- REMAINING");
    }
    lv_obj_set_style_text_color(usage_remaining_label, color(kWhite),
                                LV_PART_MAIN);
  }

  if (primary.resets_at <= 0) {
    lv_label_set_text(usage_reset_label, "RESET TIME UNKNOWN");
    lv_obj_set_style_text_color(usage_reset_label, color(kMuted),
                                LV_PART_MAIN);
    lv_label_set_text(usage_reset_time_label, "--/-- --:--");
  } else {
    lv_label_set_text(usage_reset_time_label,
                      formatResetTime(primary.resets_at,
                                      current_state.utc_offset_minutes)
                          .c_str());
    if (!current_state.time_valid) {
      lv_label_set_text(usage_reset_label, "RESETS AT");
      lv_obj_set_style_text_color(usage_reset_label, color(kMuted),
                                  LV_PART_MAIN);
    } else {
      const int64_t remaining =
          primary.resets_at - static_cast<int64_t>(
                                  current_state.unix_time + current_elapsed);
      if (remaining <= 0) {
        lv_label_set_text(usage_reset_label, "WAITING FOR REFRESH");
        lv_obj_set_style_text_color(usage_reset_label, color(kAmber),
                                    LV_PART_MAIN);
      } else {
        lv_label_set_text_fmt(usage_reset_label, "RESETS IN %s",
                              formatCountdown(remaining).c_str());
        lv_obj_set_style_text_color(usage_reset_label, color(kMuted),
                                    LV_PART_MAIN);
      }
    }
  }

  for (size_t index = 0; index < 3; ++index) {
    const size_t window_index = index + 1;
    if (window_index >= provider.window_count) {
      lv_obj_add_flag(usage_secondary_labels[index], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    const QuotaWindowState& window = provider.windows[window_index];
    lv_label_set_text(
        usage_secondary_labels[index],
        secondaryWindowText(window, current_state, current_elapsed).c_str());
    lv_obj_set_style_text_color(
        usage_secondary_labels[index], color(windowColor(window)),
        LV_PART_MAIN);
    lv_obj_clear_flag(usage_secondary_labels[index], LV_OBJ_FLAG_HIDDEN);
  }
}

void updateSessionsPage() {
  if (!current_state.data_valid) {
    lv_label_set_text(session_count_label, "--");
    lv_label_set_text(session_tokens_label, "--");
    lv_label_set_text(session_cost_label, "$--");
    lv_label_set_text(top_model_label, "-");
    lv_label_set_text(top_model_calls_label, "-- CALLS");
    return;
  }

  const uint64_t total_tokens = current_state.total_input +
                                current_state.total_output +
                                current_state.total_cache_read +
                                current_state.total_reasoning;
  lv_label_set_text_fmt(session_count_label, "%u",
                        static_cast<unsigned int>(
                            current_state.session_count));
  lv_label_set_text(session_tokens_label,
                    formatCompactTokens(total_tokens).c_str());
  lv_label_set_text(session_cost_label,
                    formatCost(current_state.total_cost_micros).c_str());
  lv_label_set_text(top_model_label, current_state.top_model);
  lv_label_set_text_fmt(top_model_calls_label, "%lu CALLS",
                        static_cast<unsigned long>(
                            current_state.top_model_calls));
}

void updateContextPage() {
  const bool valid = current_state.data_valid;
  lv_arc_set_value(cache_arc,
                   valid ? std::min<uint16_t>(
                               current_state.cache_hit_permille, 1000)
                         : 0);
  if (!valid) {
    lv_label_set_text(cache_value_label, "--.-");
    lv_label_set_text(context_input_label, "INPUT --");
    lv_label_set_text(context_output_label, "OUTPUT --");
    lv_label_set_text(context_cache_label, "CACHE --");
    lv_label_set_text(context_reasoning_label, "REASON --");
    lv_label_set_text(context_total_label, "TOTAL -- TOKENS");
    for (size_t index = 0; index < 4; ++index) {
      lv_obj_set_width(mix_segments[index], 0);
    }
    return;
  }

  lv_label_set_text_fmt(
      cache_value_label, "%u.%u",
      static_cast<unsigned int>(current_state.cache_hit_permille / 10),
      static_cast<unsigned int>(current_state.cache_hit_permille % 10));
  lv_obj_align_to(cache_value_label, cache_arc, LV_ALIGN_CENTER, 0, -18);
  lv_label_set_text_fmt(context_input_label, "INPUT %s",
                        formatCompactTokens(current_state.total_input)
                            .c_str());
  lv_label_set_text_fmt(context_output_label, "OUTPUT %s",
                        formatCompactTokens(current_state.total_output)
                            .c_str());
  lv_label_set_text_fmt(context_cache_label, "CACHE %s",
                        formatCompactTokens(current_state.total_cache_read)
                            .c_str());
  lv_label_set_text_fmt(context_reasoning_label, "REASON %s",
                        formatCompactTokens(current_state.total_reasoning)
                            .c_str());

  const uint64_t total = current_state.total_input +
                         current_state.total_output +
                         current_state.total_cache_read +
                         current_state.total_reasoning;
  lv_label_set_text_fmt(context_total_label, "TOTAL %s TOKENS",
                        formatCompactTokens(total).c_str());

  const uint64_t parts[] = {current_state.total_input,
                            current_state.total_output,
                            current_state.total_cache_read,
                            current_state.total_reasoning};
  int x = 83;
  int remaining_width = 300;
  for (size_t index = 0; index < 4; ++index) {
    int width = 0;
    if (total > 0) {
      width = index == 3
                  ? remaining_width
                  : static_cast<int>(300ULL * parts[index] / total);
    }
    lv_obj_set_pos(mix_segments[index], x, 216);
    lv_obj_set_width(mix_segments[index], width);
    x += width;
    remaining_width -= width;
  }
}

void updateConnection() {
  const ConnectionState connection = current_state.connection;
  const bool linked = connection == ConnectionState::kLinked;
  const bool stale = connection == ConnectionState::kStale;
  const bool syncing = connection == ConnectionState::kSyncing;
  const char* status = linked     ? "ONLINE"
                       : stale    ? "STALE"
                       : syncing  ? "SYNCING"
                                  : "OFFLINE";
  lv_label_set_text_fmt(connection_label, LV_SYMBOL_BLUETOOTH " %s", status);
  lv_obj_set_style_text_color(
      connection_label,
      color(linked ? kGreen : (stale ? kAmber : (syncing ? kBlue : kRed))),
      LV_PART_MAIN);

  const bool dim = !linked && !syncing;
  if (dim) {
    lv_obj_clear_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace

void uiCreate(UiActionHandler handler) {
  action_handler = handler;

  lv_obj_t* screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, color(0x030706), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t* brand_label =
      createCenteredLabel(screen, 10, "MAGPIE", &lv_font_montserrat_18,
                          kGreen, 240);
  lv_obj_set_style_text_letter_space(brand_label, 2, LV_PART_MAIN);

  connection_label =
      createCenteredLabel(screen, 31, LV_SYMBOL_BLUETOOTH " OFFLINE",
                          &lv_font_montserrat_12, kRed, 280);
  lv_obj_add_flag(connection_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(connection_label, 14);
  lv_obj_add_event_cb(
      connection_label, dispatchAction, LV_EVENT_CLICKED,
      reinterpret_cast<void*>(static_cast<uintptr_t>(UiAction::kReconnect)));

  page_title_label =
      createCenteredLabel(screen, 49, pageTitle(0),
                          &lv_font_montserrat_14, kMuted, 340);

  tileview = lv_tileview_create(screen);
  lv_obj_set_pos(tileview, 0, kPageTop);
  lv_obj_set_size(tileview, kPageWidth, kPageHeight);
  lv_obj_set_style_bg_opa(tileview, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(tileview, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(tileview, 0, LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_dir(tileview, LV_DIR_NONE);
  pages[0] = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_NONE);
  pages[1] = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_NONE);
  pages[2] = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_NONE);
  pages[3] = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_NONE);
  for (lv_obj_t* page : pages) {
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
  }
  createOverviewPage(pages[0]);
  createUsagePage(pages[1]);
  createSessionsPage(pages[2]);
  createContextPage(pages[3]);
  lv_obj_add_event_cb(tileview, displayPageChanged, LV_EVENT_VALUE_CHANGED,
                      nullptr);

  dim_overlay = lv_obj_create(screen);
  lv_obj_remove_style_all(dim_overlay);
  lv_obj_set_pos(dim_overlay, 0, kPageTop);
  lv_obj_set_size(dim_overlay, kPageWidth, kPageHeight);
  lv_obj_set_style_bg_color(dim_overlay, color(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(dim_overlay, LV_OPA_40, LV_PART_MAIN);
  lv_obj_clear_flag(dim_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(dim_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);

  for (size_t index = 0; index < kPageCount; ++index) {
    page_dots[index] = lv_obj_create(screen);
    lv_obj_remove_style_all(page_dots[index]);
    lv_obj_set_pos(page_dots[index],
                   209 + static_cast<int>(index) * 14, 440);
    lv_obj_set_size(page_dots[index], 7, 7);
    lv_obj_set_style_radius(page_dots[index], LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(page_dots[index],
                              color(index == 0 ? kGreen : kMuted),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(page_dots[index],
                            index == 0 ? LV_OPA_COVER : LV_OPA_50,
                            LV_PART_MAIN);
  }

  for (size_t index = 0; index < kMaxProviders; ++index) {
    provider_dots[index] = lv_obj_create(screen);
    lv_obj_remove_style_all(provider_dots[index]);
    lv_obj_set_size(provider_dots[index], 7, 7);
    lv_obj_set_style_radius(provider_dots[index], LV_RADIUS_CIRCLE,
                            LV_PART_MAIN);
    lv_obj_set_style_bg_color(provider_dots[index], color(kMuted),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(provider_dots[index], LV_OPA_50, LV_PART_MAIN);
    lv_obj_add_flag(provider_dots[index], LV_OBJ_FLAG_HIDDEN);
  }

  battery_label =
      createCenteredLabel(screen, 412, "BAT --%", &lv_font_montserrat_12,
                          kMuted, 160);
  lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);

  quick_actions_overlay = lv_obj_create(screen);
  lv_obj_set_size(quick_actions_overlay, 466, 466);
  lv_obj_set_pos(quick_actions_overlay, 0, 0);
  lv_obj_set_style_bg_color(quick_actions_overlay, color(0x000000),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(quick_actions_overlay, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_border_width(quick_actions_overlay, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(quick_actions_overlay, 0, LV_PART_MAIN);
  lv_obj_clear_flag(quick_actions_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* overlay_title =
      createCenteredLabel(quick_actions_overlay, 90, "GATEWAY ACTIONS",
                          &lv_font_montserrat_18, kMuted, 320);
  lv_obj_align(overlay_title, LV_ALIGN_TOP_MID, 0, 90);
  createActionButton(quick_actions_overlay, "FOCUS MAGPIE", 140,
                     UiAction::kFocusMagpie);
  createActionButton(quick_actions_overlay, "REFRESH", 206,
                     UiAction::kRefresh);
  lv_obj_t* cancel = lv_btn_create(quick_actions_overlay);
  lv_obj_set_pos(cancel, 73, 272);
  lv_obj_set_size(cancel, 320, 54);
  lv_obj_set_style_radius(cancel, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(cancel, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_color(cancel, color(kDivider), LV_PART_MAIN);
  lv_obj_set_style_border_width(cancel, 1, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(cancel, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(cancel, closeOverlay, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* cancel_label =
      createLabel(cancel, "CANCEL", &lv_font_montserrat_18, kMuted);
  lv_obj_center(cancel_label);
  lv_obj_add_flag(quick_actions_overlay, LV_OBJ_FLAG_HIDDEN);

  pairing_overlay = lv_obj_create(screen);
  lv_obj_set_size(pairing_overlay, 360, 230);
  lv_obj_align(pairing_overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(pairing_overlay, 18, LV_PART_MAIN);
  lv_obj_set_style_bg_color(pairing_overlay, color(0x061014), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(pairing_overlay, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(pairing_overlay, color(kGreen), LV_PART_MAIN);
  lv_obj_set_style_border_width(pairing_overlay, 1, LV_PART_MAIN);
  lv_obj_clear_flag(pairing_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(pairing_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(pairing_overlay, closePairingOverlay, LV_EVENT_CLICKED,
                      nullptr);
  createCenteredLabel(pairing_overlay, 30, "PAIR MAGPIE DISPLAY",
                      &lv_font_montserrat_18, kMuted, 320);
  pairing_code_label =
      createCenteredLabel(pairing_overlay, 84, "000 000",
                          &lv_font_montserrat_48, kWhite, 320);
  createCenteredLabel(pairing_overlay, 180, "ENTER ON YOUR MAC",
                      &lv_font_montserrat_12, kMuted, 320);
  lv_obj_add_flag(pairing_overlay, LV_OBJ_FLAG_HIDDEN);

  toast = createLabel(screen, "", &lv_font_montserrat_12, kWhite);
  lv_obj_set_style_bg_color(toast, color(0x182024), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(toast, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(toast, 18, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(toast, 12, LV_PART_MAIN);
  lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -76);
  lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
  hideScrollbars(screen);
}

void uiUpdate(const AppState& state, uint32_t elapsed_seconds) {
  current_state = state;
  current_elapsed = elapsed_seconds;
  has_state = true;

  selectProvider();
  updateConnection();
  updateOverview();
  updateUsagePage();
  updateSessionsPage();
  updateContextPage();
  refreshPageDots();
  refreshProviderDots();
}

void uiUpdateBattery(int percent, bool charging) {
  if (percent < 0 || percent > 100) {
    lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_label_set_text_fmt(battery_label, charging ? "CHG %d%%" : "BAT %d%%",
                        percent);
  lv_obj_set_style_text_color(
      battery_label,
      color(charging ? kBlue : (percent < 20 ? kAmber : kMuted)),
      LV_PART_MAIN);
  lv_obj_clear_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
}

void uiShowQuickActions() {
  lv_obj_clear_flag(quick_actions_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(quick_actions_overlay);
}

void uiSwipePage(int direction) {
  if (tileview == nullptr || direction == 0) {
    return;
  }
  const int current = static_cast<int>(activePageIndex());
  const int next = current + direction;
  if (next < 0 || next >= static_cast<int>(kPageCount)) {
    return;
  }
  lv_obj_set_tile(tileview, pages[next], LV_ANIM_ON);
  displayPageChanged(nullptr);
}

void uiSwipeProvider(int direction) {
  if (!has_state || direction == 0 || current_state.provider_count <= 1) {
    return;
  }
  const int next = static_cast<int>(selected_provider_index) + direction;
  if (next < 0 || next >= current_state.provider_count) {
    return;
  }
  selected_provider_index = static_cast<size_t>(next);
  strlcpy(selected_provider_id,
          current_state.providers[selected_provider_index].id,
          sizeof(selected_provider_id));

  lv_anim_del(usage_content, nullptr);
  lv_obj_set_y(usage_content, 0);
  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, usage_content);
  lv_anim_set_values(&animation, direction > 0 ? 18 : -18, 0);
  lv_anim_set_time(&animation, 180);
  lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(
      &animation, [](void* object, int32_t value) {
        lv_obj_set_y(static_cast<lv_obj_t*>(object), value);
      });
  lv_anim_start(&animation);

  updateUsagePage();
  refreshProviderDots();
}

bool uiUsagePageActive() {
  return activePageIndex() == 1;
}

void uiCyclePage() {
  if (tileview == nullptr) {
    return;
  }
  const size_t current = activePageIndex();
  const size_t next = (current + 1) % kPageCount;
  lv_obj_set_tile(tileview, pages[next], LV_ANIM_ON);
  displayPageChanged(nullptr);
}

bool uiModalVisible() {
  return (quick_actions_overlay != nullptr &&
          !lv_obj_has_flag(quick_actions_overlay, LV_OBJ_FLAG_HIDDEN)) ||
         (pairing_overlay != nullptr &&
          !lv_obj_has_flag(pairing_overlay, LV_OBJ_FLAG_HIDDEN));
}

void uiShowToast(const char* text) {
  lv_label_set_text(toast, text);
  lv_obj_clear_flag(toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -76);
  lv_obj_move_foreground(toast);
  if (toast_timer != nullptr) {
    lv_timer_del(toast_timer);
  }
  toast_timer = lv_timer_create(hideToast, 2000, nullptr);
  lv_timer_set_repeat_count(toast_timer, 1);
}

void uiShowPairingCode(uint32_t passkey) {
  lv_label_set_text_fmt(pairing_code_label, "%03lu %03lu",
                        static_cast<unsigned long>(passkey / 1000UL),
                        static_cast<unsigned long>(passkey % 1000UL));
  lv_obj_clear_flag(pairing_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(pairing_overlay);
}

void uiHidePairingCode() {
  lv_obj_add_flag(pairing_overlay, LV_OBJ_FLAG_HIDDEN);
}

bool uiPairingCodeVisible() {
  return !lv_obj_has_flag(pairing_overlay, LV_OBJ_FLAG_HIDDEN);
}
