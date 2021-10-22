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

#pragma once

#include <spdlog/spdlog.h>
#include <StupidHTTPDownloader/Downloader.h>

#include <functional>
#include <string>
#include <regex>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <future>
#include <map>

#include <process.hpp>

class UpdateChecker_Private {
 public:
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
            std::string installerExec = "../maintenancetool.exe";
        #else
            std::string installerExec = "maintenancetool";
        #endif

        auto installerPath = installerDir /= installerExec;
        return installerPath;
    }
};

class UpdateChecker : private UpdateChecker_Private {
 public:
    explicit UpdateChecker(const std::string &remoteManifestURL) : _remoteManifestURL(remoteManifestURL) {}

    enum class CheckCode {
        Succeeded = 0,
        UnspecifiedFail,
        NoRemoteURL,
        LocalManifestFetch,
        LocalManifestRead,
        RemoteManifestFetch,
        RemoteManifestRead
    };

    struct CheckResults {
        CheckCode result = CheckCode::UnspecifiedFail;
        bool hasNewerVersion = false;
    };

    std::future<CheckResults> isNewerVersionAvailable() const {
        return std::async(&UpdateChecker::_isNewerVersionAvailable, this);
    }

    std::string _getRemoteManifestContent() const {
        // else, try to download it
        spdlog::info("UpdateChecker : Downloading remote manifest [{}]", _remoteManifestURL);
        return Downloader::dumbGet(_remoteManifestURL).messageBody;
    }

    // returns if successfully requested updater to run
    static bool tryToLaunchUpdater(const std::filesystem::path &updaterPath = _exectedMaintenanceToolPath()) {
        if (!std::filesystem::exists(updaterPath)) {
            spdlog::warn("UpdateChecker : Cannot find updater at [{}], aborting", updaterPath.string());
            return false;
        }

        // run
        std::vector<std::wstring> args {updaterPath.wstring(), L"--updater"};
        spdlog::info("UpdateChecker : Launching updater [{}] ...", updaterPath.string());
        TinyProcessLib::Process run(args);

        spdlog::info("UpdateChecker : Quitting ...");
        return true;
    }

 private:
    const std::string _remoteManifestURL;

    CheckResults _isNewerVersionAvailable() const {
        //
        spdlog::info("UpdateChecker : Checking updates...");

        // if no remoteManifest given, skip
        if(_remoteManifestURL.empty()) {
            spdlog::warn("UpdateChecker : no remote manifest url configured !");
            return { CheckCode::NoRemoteURL };
        }

        // fetch local
        auto localRaw = _getLocalManifestContent();
        if (localRaw.empty()) {
            _manifestFetchingFailed("local");
            return { CheckCode::LocalManifestFetch };
        }

        auto localComponents = _extractVersionsFromManifest(localRaw);
        if (!localComponents.size()) {
            _manifestFetchingFailed("local");
            return { CheckCode::LocalManifestRead };
        }

        // fetch remote
        auto remoteRaw = _getRemoteManifestContent();
        if (remoteRaw.empty()) {
            _manifestFetchingFailed("remote");
            return { CheckCode::RemoteManifestFetch };
        }

        auto remoteVersions = _extractVersionsFromManifest(remoteRaw);
        if (!remoteVersions.size()) {
            _manifestFetchingFailed("remote");
            return { CheckCode::RemoteManifestRead };
        }

        // iterate through local components
        for (auto &[component, localVersion] : localComponents) {
            // if local component not found on remote, needs update
            auto foundOnRemote = remoteVersions.find(component);
            if (foundOnRemote == remoteVersions.end()) {
                spdlog::info("UpdateChecker : Local component [{}] not found on remote", component);
                return {CheckCode::Succeeded, true};
            }

            // compare versions
            auto &remoteVersion = foundOnRemote->second;
            auto isNewer = _isRemoteVersionNewerThanLocal(localVersion, remoteVersion);
            if (isNewer) {
                spdlog::info("UpdateChecker : Local component [{} : {}] older than remote [{}]", component, localVersion, remoteVersion);
                return {CheckCode::Succeeded, true};
            }

            // if not older, remove from remote
            spdlog::info("UpdateChecker : Local component [{}] up-to-date", component);
            remoteVersions.erase(component);
        }

        // if any components remaining in remote, has updates
        if (remoteVersions.size()) {
            auto &firstComponent = remoteVersions.begin()->first;
            spdlog::info("UpdateChecker : Remote component [{}] not found in local", firstComponent);
            return {CheckCode::Succeeded, true};
        }

        //
        spdlog::info("UpdateChecker : No components to be updated");
        return {CheckCode::Succeeded, false};
    }
    
    static void _manifestFetchingFailed(const char* manifestType) {
        spdlog::warn("UpdateChecker : Error while fetching {} manifest !", manifestType);
    }
};
