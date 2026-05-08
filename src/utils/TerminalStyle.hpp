#pragma once

#include <iosfwd>
#include <string>
#include <string_view>

namespace cfgsync::utils {

enum class TerminalColor {
    Default,
    Red,
    Green,
    Yellow,
    Cyan,
};

class TerminalStyle {
public:
    static TerminalStyle Plain();
    static TerminalStyle Foreground(TerminalColor color);

    TerminalStyle Bold() const;

    TerminalColor GetForeground() const;
    bool IsBold() const;
    bool IsPlain() const;

private:
    TerminalColor Foreground_ = TerminalColor::Default;
    bool Bold_ = false;
};

class Colorizer {
public:
    explicit Colorizer(bool enabled = true);

    static Colorizer Enabled();
    static Colorizer Disabled();

    bool IsEnabled() const;
    std::string Apply(std::string_view text, TerminalStyle style) const;

private:
    bool Enabled_;
};

std::ostream& WriteStyled(std::ostream& output, std::string_view text, TerminalStyle style,
                          const Colorizer& colorizer = Colorizer{});

}  // namespace cfgsync::utils
