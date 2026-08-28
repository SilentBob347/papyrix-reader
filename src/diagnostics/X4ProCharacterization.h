#pragma once

#include <string_view>

namespace papyrix::diagnostics {

enum class X4ProCommand {
  None,
  Status,
  LightOff,
  CoolLight,
  WarmLight,
  Sleep,
  PanelProbe,
  WhiteRefresh,
};

constexpr X4ProCommand parseX4ProCommand(std::string_view line) {
  if (line == "?") return X4ProCommand::Status;
  if (line == "0") return X4ProCommand::LightOff;
  if (line == "1") return X4ProCommand::CoolLight;
  if (line == "2") return X4ProCommand::WarmLight;
  if (line == "s") return X4ProCommand::Sleep;
  if (line == "p") return X4ProCommand::PanelProbe;
  if (line == "white") return X4ProCommand::WhiteRefresh;
  return X4ProCommand::None;
}

void beginX4ProCharacterization();
void updateX4ProCharacterization();

}  // namespace papyrix::diagnostics
