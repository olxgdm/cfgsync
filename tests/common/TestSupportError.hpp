#pragma once

#include <stdexcept>

namespace cfgsync::tests {

class TestSupportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace cfgsync::tests
