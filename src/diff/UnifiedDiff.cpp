#include "diff/UnifiedDiff.hpp"

#include "Exceptions.hpp"
#include "utils/TerminalStyle.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace cfgsync::diff {
namespace fs = std::filesystem;

namespace {

constexpr std::size_t ContextLineCount = 3;

struct DiffLine {
    std::string Text;
    bool HasTrailingNewline;
};

enum class OperationType {
    Context,
    Delete,
    Insert,
};

struct DiffOperation {
    OperationType Type;
    DiffLine Line;
    std::size_t OldIndex;
    std::size_t NewIndex;
};

struct HunkRange {
    std::size_t Start;
    std::size_t End;
};

bool IsSupportedText(std::string_view text) { return text.find('\0') == std::string_view::npos; }

void RequireSupportedText(std::string_view text, std::string_view label) {
    if (!IsSupportedText(text)) {
        throw FileError{
            fmt::format(fmt::runtime("Binary diff is unsupported for file containing NUL bytes: {}"), label)};
    }
}

std::vector<DiffLine> SplitLines(std::string_view text) {
    std::vector<DiffLine> lines;
    std::size_t lineStart = 0;

    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] != '\n') {
            continue;
        }

        lines.push_back({
            .Text = std::string{text.substr(lineStart, index - lineStart)},
            .HasTrailingNewline = true,
        });
        lineStart = index + 1;
    }

    if (lineStart < text.size()) {
        lines.push_back({
            .Text = std::string{text.substr(lineStart)},
            .HasTrailingNewline = false,
        });
    }

    return lines;
}

bool LinesEqual(const DiffLine& left, const DiffLine& right) {
    return left.Text == right.Text && left.HasTrailingNewline == right.HasTrailingNewline;
}

std::vector<DiffOperation> BuildOperations(const std::vector<DiffLine>& oldLines,
                                           const std::vector<DiffLine>& newLines) {
    const auto oldCount = oldLines.size();
    const auto newCount = newLines.size();
    std::vector<std::vector<std::size_t>> lcs(oldCount + 1, std::vector<std::size_t>(newCount + 1, 0));

    for (std::size_t oldIndex = oldCount; oldIndex > 0; --oldIndex) {
        for (std::size_t newIndex = newCount; newIndex > 0; --newIndex) {
            if (LinesEqual(oldLines[oldIndex - 1], newLines[newIndex - 1])) {
                lcs[oldIndex - 1][newIndex - 1] = lcs[oldIndex][newIndex] + 1;
            } else {
                lcs[oldIndex - 1][newIndex - 1] = std::max(lcs[oldIndex][newIndex - 1], lcs[oldIndex - 1][newIndex]);
            }
        }
    }

    std::vector<DiffOperation> operations;
    auto oldIndex = std::size_t{0};
    auto newIndex = std::size_t{0};

    while (oldIndex < oldCount && newIndex < newCount) {
        if (LinesEqual(oldLines[oldIndex], newLines[newIndex])) {
            operations.push_back({
                .Type = OperationType::Context,
                .Line = oldLines[oldIndex],
                .OldIndex = oldIndex,
                .NewIndex = newIndex,
            });
            ++oldIndex;
            ++newIndex;
            continue;
        }

        if (lcs[oldIndex + 1][newIndex] >= lcs[oldIndex][newIndex + 1]) {
            operations.push_back({
                .Type = OperationType::Delete,
                .Line = oldLines[oldIndex],
                .OldIndex = oldIndex,
                .NewIndex = newIndex,
            });
            ++oldIndex;
        } else {
            operations.push_back({
                .Type = OperationType::Insert,
                .Line = newLines[newIndex],
                .OldIndex = oldIndex,
                .NewIndex = newIndex,
            });
            ++newIndex;
        }
    }

    while (oldIndex < oldCount) {
        operations.push_back({
            .Type = OperationType::Delete,
            .Line = oldLines[oldIndex],
            .OldIndex = oldIndex,
            .NewIndex = newIndex,
        });
        ++oldIndex;
    }

    while (newIndex < newCount) {
        operations.push_back({
            .Type = OperationType::Insert,
            .Line = newLines[newIndex],
            .OldIndex = oldIndex,
            .NewIndex = newIndex,
        });
        ++newIndex;
    }

    return operations;
}

std::vector<HunkRange> BuildHunkRanges(const std::vector<DiffOperation>& operations) {
    std::vector<HunkRange> hunks;

    for (std::size_t index = 0; index < operations.size(); ++index) {
        if (operations[index].Type == OperationType::Context) {
            continue;
        }

        const auto start = index > ContextLineCount ? index - ContextLineCount : 0;
        const auto end = std::min(operations.size() - 1, index + ContextLineCount);
        if (!hunks.empty() && start <= hunks.back().End + 1) {
            hunks.back().End = std::max(hunks.back().End, end);
            continue;
        }

        hunks.push_back({
            .Start = start,
            .End = end,
        });
    }

    return hunks;
}

bool HasOldLine(const DiffOperation& operation) {
    return operation.Type == OperationType::Context || operation.Type == OperationType::Delete;
}

bool HasNewLine(const DiffOperation& operation) {
    return operation.Type == OperationType::Context || operation.Type == OperationType::Insert;
}

std::size_t CountOldLinesBefore(const std::vector<DiffOperation>& operations, std::size_t endIndex) {
    return static_cast<std::size_t>(std::count_if(operations.begin(), operations.begin() + endIndex, HasOldLine));
}

std::size_t CountNewLinesBefore(const std::vector<DiffOperation>& operations, std::size_t endIndex) {
    return static_cast<std::size_t>(std::count_if(operations.begin(), operations.begin() + endIndex, HasNewLine));
}

std::size_t CountOldLinesInRange(const std::vector<DiffOperation>& operations, const HunkRange& hunk) {
    return static_cast<std::size_t>(
        std::count_if(operations.begin() + hunk.Start, operations.begin() + hunk.End + 1, HasOldLine));
}

std::size_t CountNewLinesInRange(const std::vector<DiffOperation>& operations, const HunkRange& hunk) {
    return static_cast<std::size_t>(
        std::count_if(operations.begin() + hunk.Start, operations.begin() + hunk.End + 1, HasNewLine));
}

std::size_t HunkStartLine(const std::vector<DiffOperation>& operations, const HunkRange& hunk, bool oldSide,
                          std::size_t count) {
    if (count == 0) {
        return oldSide ? CountOldLinesBefore(operations, hunk.Start) : CountNewLinesBefore(operations, hunk.Start);
    }

    for (std::size_t index = hunk.Start; index <= hunk.End; ++index) {
        if (oldSide && HasOldLine(operations[index])) {
            return operations[index].OldIndex + 1;
        }
        if (!oldSide && HasNewLine(operations[index])) {
            return operations[index].NewIndex + 1;
        }
    }

    return 0;
}

char OperationPrefix(OperationType type) {
    switch (type) {
        case OperationType::Context:
            return ' ';
        case OperationType::Delete:
            return '-';
        case OperationType::Insert:
            return '+';
    }

    return ' ';
}

void AppendOperation(std::ostringstream& output, const DiffOperation& operation) {
    output << OperationPrefix(operation.Type) << operation.Line.Text << '\n';
    if (!operation.Line.HasTrailingNewline) {
        output << "\\ No newline at end of file\n";
    }
}

void AppendHunk(std::ostringstream& output, const std::vector<DiffOperation>& operations, const HunkRange& hunk) {
    const auto oldCount = CountOldLinesInRange(operations, hunk);
    const auto newCount = CountNewLinesInRange(operations, hunk);
    const auto oldStart = HunkStartLine(operations, hunk, true, oldCount);
    const auto newStart = HunkStartLine(operations, hunk, false, newCount);

    output << fmt::format(fmt::runtime("@@ -{},{} +{},{} @@\n"), oldStart, oldCount, newStart, newCount);

    for (std::size_t index = hunk.Start; index <= hunk.End; ++index) {
        AppendOperation(output, operations[index]);
    }
}

utils::TerminalStyle StyleForUnifiedDiffLine(std::string_view line) {
    if (line.starts_with("---") || line.starts_with("+++")) {
        return utils::TerminalStyle::Foreground(utils::TerminalColor::Cyan).Bold();
    }

    if (line.starts_with("@@")) {
        return utils::TerminalStyle::Foreground(utils::TerminalColor::Yellow);
    }

    if (line.starts_with("-")) {
        return utils::TerminalStyle::Foreground(utils::TerminalColor::Red);
    }

    if (line.starts_with("+")) {
        return utils::TerminalStyle::Foreground(utils::TerminalColor::Green);
    }

    return utils::TerminalStyle::Plain();
}

}  // namespace

std::string ReadTextFileForDiff(const fs::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw FileError{fmt::format(fmt::runtime("Unable to open file for diff: {}"), path.string())};
    }

    const std::string contents{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    RequireSupportedText(contents, path.string());
    return contents;
}

std::string GenerateUnifiedDiff(std::string_view oldText, std::string_view newText, std::string_view oldLabel,
                                std::string_view newLabel) {
    RequireSupportedText(oldText, oldLabel);
    RequireSupportedText(newText, newLabel);

    if (oldText == newText) {
        return {};
    }

    const auto oldLines = SplitLines(oldText);
    const auto newLines = SplitLines(newText);
    const auto operations = BuildOperations(oldLines, newLines);
    const auto hunks = BuildHunkRanges(operations);

    if (hunks.empty()) {
        return {};
    }

    std::ostringstream output;
    output << "--- " << oldLabel << '\n';
    output << "+++ " << newLabel << '\n';

    for (const auto& hunk : hunks) {
        AppendHunk(output, operations, hunk);
    }

    return output.str();
}

std::string RenderUnifiedDiff(std::string_view diffText, const utils::Colorizer& colorizer) {
    std::ostringstream output;
    std::size_t lineStart = 0;

    while (lineStart < diffText.size()) {
        const auto lineEnd = diffText.find('\n', lineStart);
        const auto hasLineEnding = lineEnd != std::string_view::npos;
        const auto contentEnd = hasLineEnding ? lineEnd : diffText.size();
        const auto line = diffText.substr(lineStart, contentEnd - lineStart);

        utils::WriteStyled(output, line, StyleForUnifiedDiffLine(line), colorizer);
        if (hasLineEnding) {
            output << '\n';
            lineStart = lineEnd + 1;
        } else {
            lineStart = diffText.size();
        }
    }

    return output.str();
}

}  // namespace cfgsync::diff
