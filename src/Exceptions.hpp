#pragma once

#include <stdexcept>

namespace cfgsync {

class CfgsyncError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConfigError : public CfgsyncError {
public:
    using CfgsyncError::CfgsyncError;
};

class RegistryError : public CfgsyncError {
public:
    using CfgsyncError::CfgsyncError;
};

class FileError : public CfgsyncError {
public:
    using CfgsyncError::CfgsyncError;
};

class CommandError : public CfgsyncError {
public:
    using CfgsyncError::CfgsyncError;
};

}  // namespace cfgsync
