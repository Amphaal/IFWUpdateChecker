// IFWUpdateChecker
// In-app simple helper to check if a Qt IFW package has updates
// Copyright (C) 2021 Guillaume Vara <guillaume.vara@gmail.com>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Any graphical resources available within the source code may
// use a different license and copyright : please refer to their metadata
// for further details. Graphical resources without explicit references to a
// different license and copyright still refer to this GPL.

#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <catch2/catch.hpp>

#include "IFWUpdateChecker.hpp"

TEST_CASE("Version comparaison", "[update checker]") {
    spdlog::set_level(spdlog::level::debug);

    std::string test_localVersion("0.5.0");
    auto testUpdateChecker = [test_localVersion](const std::string &remoteVersion) {
        return UpdateChecker_Private::_isRemoteVersionNewerThanLocal(test_localVersion, remoteVersion);
    };

    REQUIRE(testUpdateChecker("0.5.1"));
    REQUIRE(testUpdateChecker("0.5.10"));
    REQUIRE(testUpdateChecker("1.0"));

    REQUIRE_FALSE(testUpdateChecker("0.0.1"));
    REQUIRE_FALSE(testUpdateChecker("0.5.0"));
    REQUIRE_FALSE(testUpdateChecker("0.4.10"));
}
