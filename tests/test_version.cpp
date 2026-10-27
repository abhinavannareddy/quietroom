#include <gtest/gtest.h>

#include <string>

#include "quietroom/version.hpp"

TEST(Version, IsNotEmpty) { EXPECT_FALSE(std::string(quietroom::version()).empty()); }
