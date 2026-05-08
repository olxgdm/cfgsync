#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace cfgsync::utils {
class Colorizer;
}

namespace cfgsync::diff {

std::string ReadTextFileForDiff(const std::filesystem::path& path);

std::string GenerateUnifiedDiff(std::string_view oldText, std::string_view newText, std::string_view oldLabel,
                                std::string_view newLabel);

std::string RenderUnifiedDiff(std::string_view diffText, const utils::Colorizer& colorizer);

}  // namespace cfgsync::diff
