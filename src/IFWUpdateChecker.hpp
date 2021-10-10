// IFWUpdateChecker
// Check if an IFW package has updates
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

#pragma once

#include <spdlog/spdlog.h>

#include <functional>
#include <string>
#include <regex>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <future>
#include <map>

#include <StupidHTTPDownloader.hpp>
#include <process.hpp>

class UpdateChecker_Private {
 public:
    static std::string _getRemoteManifestContent() {
        spdlog::info("UpdateChecker : Downloading remote manifest [{}, {}]", APP_REPOSITORY_HOST, APP_REPOSITORY_COMMAND);
        return StupidHTTPDownloader::DownloadHTTPFile(APP_REPOSITORY_HOST, APP_REPOSITORY_COMMAND);
    }

    static std::string _getLocalManifestContent() {
        // check existence
        auto absolute = std::filesystem::absolute("../components.xml");
        if (!std::filesystem::exists(absolute)) {
            spdlog::warn("UpdateChecker : No local manifest found at [{}]", absolute.string());
            return std::string();
        }

        // dump content
        std::ifstream t(absolute);
        std::stringstream buffer;
        buffer << t.rdbuf();
        spdlog::info("UpdateChecker : Local manifest found at [{}]", absolute.string());

        //
        return buffer.str();
    }

    static auto _extractVersionsFromManifest(std::string manifestContent) {
        // remove newline
        manifestContent.erase(std::remove(manifestContent.begin(), manifestContent.end(), '\n'), manifestContent.end());

        //
        std::regex findVersionExp(R"|(<Name>(.*?)<\/Name>.*?<Version>(.*?)<\/Version>)|");
        std::smatch match;
        std::map<std::string, std::string> out;

        //
        std::string::const_iterator searchStart(manifestContent.cbegin());
        while (std::regex_search(searchStart, manifestContent.cend(), match, findVersionExp)) {
            //
            bool first = true;
            std::string key;
            auto count = match.size();

            // iterate
            for (auto &group : match) {
                // skip first (whole pattern)
                if (first) {
                    first ^= true;
                    continue;
                }

                // store key
                if (key.empty()) {
                    key = group.str();
                    continue;
                }

                // store version
                auto version = group.str();
                out.insert_or_assign(key, version);
                break;
            }

            //
            searchStart = match.suffix().first;
        }

        //
        return out;
    }

    static bool _isRemoteVersionNewerThanLocal(const std::string &localVersion, const std::string &remoteVersion) {
        return localVersion < remoteVersion;
    }

    static std::filesystem::path _exectedMaintenanceToolPath() {
        auto installerDir = std::filesystem::current_path().parent_path();

        // TODO(amphaal) MacOS ?
        #ifdef WIN32
            std::string installerExec = "maintenancetool.exe";
        #else
            std::string installerExec = "maintenancetool";
        #endif

        auto installerPath = installerDir /= installerExec;
        return installerPath;
    }
};

class UpdateChecker : private UpdateChecker_Private {
 public:
    static std::future<bool> isNewerVersionAvailable() {
        return std::async(_isNewerVersionAvailable);
    }

    // returns if successfully requested updater to run
    static bool tryToLaunchUpdater(const std::filesystem::path &updaterPath = _exectedMaintenanceToolPath()) {
        if (!std::filesystem::exists(updaterPath)) {
            spdlog::warn("UpdateChecker : Cannot find updater at [{}], aborting", updaterPath.string());
            return false;
        }

        // run
        std::vector<std::string> args {updaterPath.string(), "--updater"};
        spdlog::info("UpdateChecker : Launching updater [{}] ...", updaterPath.string());
        TinyProcessLib::Process run(args);

        spdlog::info("UpdateChecker : Quitting {} ...", APP_NAME);
        return true;
    }

 private:
    static bool _warn(const char* str) {
        spdlog::warn("UpdateChecker : Error while fetching {} manifest !", str);
        return false;
    }

    static bool _isNewerVersionAvailable() {
        //
        spdlog::info("UpdateChecker : Checking updates...");

        // fetch local
        std::map<std::string, std::string> localComponents;
        auto localRaw = _getLocalManifestContent();
        if (localRaw.empty()) return _warn("local");
        localComponents = _extractVersionsFromManifest(localRaw);
        if (!localComponents.size()) return _warn("local");

        // fetch remote
        std::map<std::string, std::string> remoteVersions;
        auto remoteRaw = _getRemoteManifestContent();
        if (remoteRaw.empty()) return _warn("remote");
        remoteVersions = _extractVersionsFromManifest(remoteRaw);
        if (!remoteVersions.size()) return _warn("remote");

        // iterate through local components
        for (auto &[component, localVersion] : localComponents) {
            // if local component not found on remote, needs update
            auto foundOnRemote = remoteVersions.find(component);
            if (foundOnRemote == remoteVersions.end()) {
                spdlog::info("UpdateChecker : Local component [{}] not found on remote", component);
                return true;
            }

            // compare versions
            auto &remoteVersion = foundOnRemote->second;
            auto isNewer = _isRemoteVersionNewerThanLocal(localVersion, remoteVersion);
            if (isNewer) {
                spdlog::info("UpdateChecker : Local component [{} : {}] older than remote [{}]", component, localVersion, remoteVersion);
                return true;
            }

            // if not older, remove from remote
            spdlog::info("UpdateChecker : Local component [{}] up-to-date", component);
            remoteVersions.erase(component);
        }

        // if any components remaining in remote, has updates
        if (remoteVersions.size()) {
            auto &firstComponent = remoteVersions.begin()->first;
            spdlog::info("UpdateChecker : Remote component [{}] not found in local", firstComponent);
            return true;
        }

        //
        spdlog::info("UpdateChecker : No components to be updated");
        return false;
    }
};
