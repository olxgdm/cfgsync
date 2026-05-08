#include "utils/TerminalStyle.hpp"

#include <ostream>
#include <string>
#include <string_view>

namespace cfgsync::utils {

namespace {

constexpr char Escape = static_cast<char>(27);
constexpr char ResetSequenceText[] = {Escape, '[', '0', 'm'};
constexpr char BoldSequenceText[] = {Escape, '[', '1', 'm'};
constexpr char RedSequenceText[] = {Escape, '[', '3', '1', 'm'};
constexpr char GreenSequenceText[] = {Escape, '[', '3', '2', 'm'};
constexpr char YellowSequenceText[] = {Escape, '[', '3', '3', 'm'};
constexpr char CyanSequenceText[] = {Escape, '[', '3', '6', 'm'};

constexpr std::string_view ResetSequence{ResetSequenceText, 4};
constexpr std::string_view BoldSequence{BoldSequenceText, 4};
constexpr std::string_view RedSequence{RedSequenceText, 5};
constexpr std::string_view GreenSequence{GreenSequenceText, 5};
constexpr std::string_view YellowSequence{YellowSequenceText, 5};
constexpr std::string_view CyanSequence{CyanSequenceText, 5};

std::string_view ColorSequence(TerminalColor color) {
    switch (color) {
        case TerminalColor::Default:
            return {};
        case TerminalColor::Red:
            return RedSequence;
        case TerminalColor::Green:
            return GreenSequence;
        case TerminalColor::Yellow:
            return YellowSequence;
        case TerminalColor::Cyan:
            return CyanSequence;
    }

    return {};
}

}  // namespace

TerminalStyle TerminalStyle::Plain() { return {}; }

TerminalStyle TerminalStyle::Foreground(TerminalColor color) {
    TerminalStyle style;
    style.Foreground_ = color;
    return style;
}

TerminalStyle TerminalStyle::Bold() const {
    auto style = *this;
    style.Bold_ = true;
    return style;
}

TerminalColor TerminalStyle::GetForeground() const { return Foreground_; }

bool TerminalStyle::IsBold() const { return Bold_; }

bool TerminalStyle::IsPlain() const { return Foreground_ == TerminalColor::Default && !Bold_; }

Colorizer::Colorizer(bool enabled) : Enabled_(enabled) {}

Colorizer Colorizer::Enabled() { return Colorizer{true}; }

Colorizer Colorizer::Disabled() { return Colorizer{false}; }

bool Colorizer::IsEnabled() const { return Enabled_; }

std::string Colorizer::Apply(std::string_view text, TerminalStyle style) const {
    if (!Enabled_ || style.IsPlain()) {
        return std::string{text};
    }

    std::string styledText;
    if (style.IsBold()) {
        styledText.append(BoldSequence);
    }

    styledText.append(ColorSequence(style.GetForeground()));
    styledText.append(text);
    styledText.append(ResetSequence);
    return styledText;
}

std::ostream& WriteStyled(std::ostream& output, std::string_view text, TerminalStyle style,
                          const Colorizer& colorizer) {
    output << colorizer.Apply(text, style);
    return output;
}

}  // namespace cfgsync::utils
