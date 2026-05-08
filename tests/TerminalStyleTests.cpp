#include "common/GoogleTestMain.hpp"
#include "gtest/gtest.h"
#include "utils/TerminalStyle.hpp"

#include <sstream>
#include <string>
#include <string_view>

namespace {

std::string EscapeSequence(std::string_view suffix) {
    std::string sequence{static_cast<char>(27)};
    sequence.append(suffix);
    return sequence;
}

TEST(TerminalStyleTest, AppliesForegroundColorAndResetWhenEnabled) {
    const auto colorizer = cfgsync::utils::Colorizer::Enabled();
    const auto styledText =
        colorizer.Apply("removed", cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Red));

    EXPECT_EQ(styledText, EscapeSequence("[31m") + "removed" + EscapeSequence("[0m"));
}

TEST(TerminalStyleTest, ComposesBoldWithForegroundColor) {
    const auto colorizer = cfgsync::utils::Colorizer::Enabled();
    const auto styledText = colorizer.Apply(
        "header", cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Cyan).Bold());

    EXPECT_EQ(styledText, EscapeSequence("[1m") + EscapeSequence("[36m") + "header" + EscapeSequence("[0m"));
}

TEST(TerminalStyleTest, DisabledColorizerReturnsPlainText) {
    const auto colorizer = cfgsync::utils::Colorizer::Disabled();

    EXPECT_EQ(colorizer.Apply("added", cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Green)),
              "added");
}

TEST(TerminalStyleTest, PlainStyleDoesNotAddSequences) {
    const auto colorizer = cfgsync::utils::Colorizer::Enabled();

    EXPECT_EQ(colorizer.Apply("context", cfgsync::utils::TerminalStyle::Plain()), "context");
}

TEST(TerminalStyleTest, WriteStyledWritesThroughColorizer) {
    std::ostringstream output;

    cfgsync::utils::WriteStyled(output, "hunk",
                                cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Yellow));

    EXPECT_EQ(output.str(), EscapeSequence("[33m") + "hunk" + EscapeSequence("[0m"));
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
