#include "Exceptions.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestTempDirectory.hpp"
#include "diff/UnifiedDiff.hpp"
#include "gtest/gtest.h"
#include "utils/TerminalStyle.hpp"

#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;

TEST(UnifiedDiffTest, EmitsAddition) {
    EXPECT_EQ(cfgsync::diff::GenerateUnifiedDiff("a\nc\n", "a\nb\nc\n", "old", "new"),
              "--- old\n"
              "+++ new\n"
              "@@ -1,2 +1,3 @@\n"
              " a\n"
              "+b\n"
              " c\n");
}

TEST(UnifiedDiffTest, EmitsDeletion) {
    EXPECT_EQ(cfgsync::diff::GenerateUnifiedDiff("a\nb\nc\n", "a\nc\n", "old", "new"),
              "--- old\n"
              "+++ new\n"
              "@@ -1,3 +1,2 @@\n"
              " a\n"
              "-b\n"
              " c\n");
}

TEST(UnifiedDiffTest, EmitsModificationAsDeleteThenInsert) {
    EXPECT_EQ(cfgsync::diff::GenerateUnifiedDiff("color=blue\n", "color=green\n", "old", "new"),
              "--- old\n"
              "+++ new\n"
              "@@ -1,1 +1,1 @@\n"
              "-color=blue\n"
              "+color=green\n");
}

TEST(UnifiedDiffTest, EmitsMultipleHunks) {
    const std::string oldText =
        "line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\nline13\nline14\n"
        "line15\n";
    const std::string newText =
        "line1\nchanged2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\nline13\n"
        "changed14\nline15\n";

    EXPECT_EQ(cfgsync::diff::GenerateUnifiedDiff(oldText, newText, "old", "new"),
              "--- old\n"
              "+++ new\n"
              "@@ -1,5 +1,5 @@\n"
              " line1\n"
              "-line2\n"
              "+changed2\n"
              " line3\n"
              " line4\n"
              " line5\n"
              "@@ -11,5 +11,5 @@\n"
              " line11\n"
              " line12\n"
              " line13\n"
              "-line14\n"
              "+changed14\n"
              " line15\n");
}

TEST(UnifiedDiffTest, EmitsAdditionFromEmptyOldFile) {
    EXPECT_EQ(cfgsync::diff::GenerateUnifiedDiff("", "a\n", "old", "new"),
              "--- old\n"
              "+++ new\n"
              "@@ -0,0 +1,1 @@\n"
              "+a\n");
}

TEST(UnifiedDiffTest, EmitsDeletionToEmptyNewFile) {
    EXPECT_EQ(cfgsync::diff::GenerateUnifiedDiff("a\n", "", "old", "new"),
              "--- old\n"
              "+++ new\n"
              "@@ -1,1 +0,0 @@\n"
              "-a\n");
}

TEST(UnifiedDiffTest, IdenticalFilesProduceEmptyOutput) {
    EXPECT_TRUE(cfgsync::diff::GenerateUnifiedDiff("same\n", "same\n", "old", "new").empty());
}

TEST(UnifiedDiffTest, EmitsMissingTrailingNewlineMarker) {
    EXPECT_EQ(cfgsync::diff::GenerateUnifiedDiff("a\nb", "a\nc\n", "old", "new"),
              "--- old\n"
              "+++ new\n"
              "@@ -1,2 +1,2 @@\n"
              " a\n"
              "-b\n"
              "\\ No newline at end of file\n"
              "+c\n");
}

TEST(UnifiedDiffTest, SameLineTextWithDifferentTrailingNewlineIsDifferent) {
    EXPECT_EQ(cfgsync::diff::GenerateUnifiedDiff("a", "a\n", "old", "new"),
              "--- old\n"
              "+++ new\n"
              "@@ -1,1 +1,1 @@\n"
              "-a\n"
              "\\ No newline at end of file\n"
              "+a\n");
}

TEST(UnifiedDiffTest, RejectsNulBytesInInputText) {
    const std::string binaryText{"a\0b", 3};

    try {
        static_cast<void>(cfgsync::diff::GenerateUnifiedDiff(binaryText, "a\n", "old", "new"));
        FAIL() << "Binary text did not throw.";
    } catch (const cfgsync::FileError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Binary diff is unsupported"), std::string::npos);
        EXPECT_NE(message.find("old"), std::string::npos);
    }
}

TEST(UnifiedDiffTest, RejectsNulBytesWhenReadingFile) {
    const auto testRoot = cfgsync::tests::MakeTestRoot();
    const auto filePath = testRoot / "binary.conf";
    cfgsync::tests::WriteBinaryFile(filePath, std::string{"a\0b", 3});

    try {
        static_cast<void>(cfgsync::diff::ReadTextFileForDiff(filePath));
        FAIL() << "Binary file did not throw.";
    } catch (const cfgsync::FileError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Binary diff is unsupported"), std::string::npos);
        EXPECT_NE(message.find(filePath.string()), std::string::npos);
    }

    fs::remove_all(testRoot);
}

TEST(UnifiedDiffTest, RenderUnifiedDiffAppliesDiffLineStyles) {
    const auto renderedDiff = cfgsync::diff::RenderUnifiedDiff(
        "--- old\n+++ new\n@@ -1,1 +1,1 @@\n-context\n+changed\n unchanged\n", cfgsync::utils::Colorizer::Enabled());
    const auto colorizer = cfgsync::utils::Colorizer::Enabled();

    EXPECT_EQ(
        renderedDiff,
        colorizer.Apply("--- old",
                        cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Cyan).Bold()) +
            "\n" +
            colorizer.Apply("+++ new",
                            cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Cyan).Bold()) +
            "\n" +
            colorizer.Apply("@@ -1,1 +1,1 @@",
                            cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Yellow)) +
            "\n" +
            colorizer.Apply("-context", cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Red)) +
            "\n" +
            colorizer.Apply("+changed",
                            cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Green)) +
            "\n"
            " unchanged\n");
}

TEST(UnifiedDiffTest, RenderUnifiedDiffCanDisableColors) {
    const std::string plainDiff = "--- old\n+++ new\n@@ -1,1 +1,1 @@\n-old\n+new\n";

    EXPECT_EQ(cfgsync::diff::RenderUnifiedDiff(plainDiff, cfgsync::utils::Colorizer::Disabled()), plainDiff);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
