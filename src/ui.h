#pragma once

#include "app_state.h"

enum class UiAction : uint8_t {
  kFocusMagpie,
  kRefresh,
  kReconnect,
};

using UiActionHandler = void (*)(UiAction action);

void uiCreate(UiActionHandler action_handler);
void uiUpdate(const AppState& state, uint32_t elapsed_seconds);
void uiUpdateBattery(int percent, bool charging);
void uiCyclePage();
void uiSwipePage(int direction);
void uiSwipeProvider(int direction);
bool uiUsagePageActive();
void uiShowQuickActions();
void uiShowToast(const char* text);
void uiShowPairingCode(uint32_t passkey);
void uiHidePairingCode();
bool uiPairingCodeVisible();
bool uiModalVisible();
