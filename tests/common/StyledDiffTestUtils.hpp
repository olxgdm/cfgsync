#pragma once

#include "utils/TerminalStyle.hpp"

#include <string>
#include <string_view>

namespace cfgsync::tests {

inline std::string StyledDiffHeader(std::string_view text) {
    return utils::Colorizer::Enabled().Apply(text, utils::TerminalStyle::Foreground(utils::TerminalColor::Cyan).Bold());
}

inline std::string StyledDiffHunk(std::string_view text) {
    return utils::Colorizer::Enabled().Apply(text, utils::TerminalStyle::Foreground(utils::TerminalColor::Yellow));
}

inline std::string StyledDiffRemoved(std::string_view text) {
    return utils::Colorizer::Enabled().Apply(text, utils::TerminalStyle::Foreground(utils::TerminalColor::Red));
}

inline std::string StyledDiffAdded(std::string_view text) {
    return utils::Colorizer::Enabled().Apply(text, utils::TerminalStyle::Foreground(utils::TerminalColor::Green));
}

}  // namespace cfgsync::tests
