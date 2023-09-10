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

#include <version.h>

class UpdateChecker_Private {
 public:
    static std::string _getLocalManifestContent() {
        // check existence
        auto absolute = std::filesystem::absolute("../components.xml");
        if (!std::filesystem::exists(absolute)) {
            spdlog::warn("[IFW] UpdateChecker : No local manifest found at [{}]", absolute.string());
            return std::string();
        }

        // dump content
        std::ifstream t(absolute);
        std::stringstream buffer;
        buffer << t.rdbuf();
        spdlog::info("[IFW] UpdateChecker : Local manifest found at [{}]", absolute.string());

        //
        return buffer.str();
    }

    //
    //
    //

    static auto _extractTagNameFromGithubReleaseManifest(std::string& manifestContent) {
        return _extractVersionsFromManifestRaw(R"|(\"(tag_name)\": \"(.*?)\")|", manifestContent);
    }

    static auto _extractVersionsFromManifest(std::string manifestContent) {
        return _extractVersionsFromManifestRaw(R"|(<Name>(.*?)<\/Name>.*?<Version>(.*?)<\/Version>)|", manifestContent);
    }

    static std::map<std::string, std::string> _extractVersionsFromManifestRaw(const char* regex, std::string manifestContent) {
        // remove newline
        manifestContent.erase(std::remove(manifestContent.begin(), manifestContent.end(), '\n'), manifestContent.end());

        //
        std::regex findVersionExp(regex);
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

    //
    //
    //

    static bool _isRemoteVersionNewerThanLocal(const std::string &localVersion, const std::string &remoteVersion) {
        return localVersion < remoteVersion;
    }

    static std::filesystem::path _expectedMaintenanceToolPath() {
        auto installerDir = std::filesystem::current_path().parent_path();

        // TODO(amphaal) MacOS ?
        #ifdef _WIN32
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

    enum class CheckSource {
        GithubCheck,
        IFWCheck,
    };

    struct CheckResults {
        CheckSource source;
        CheckCode result = CheckCode::UnspecifiedFail;
        bool hasNewerVersion = false;
    };

    std::future<const CheckResults> isNewerVersionAvailableAsync() const {
        return std::async(&UpdateChecker::isNewerVersionAvailable, this);
    }

    const CheckResults isNewerVersionAvailable() const {
        return _isNewerVersionAvailable();
    }

    std::string _getRemoteManifestContent() const {
        // else, try to download it
        spdlog::info("[IFW] UpdateChecker : Downloading remote manifest [{}]", _remoteManifestURL);
        return Downloader::dumbGet(_remoteManifestURL).messageBody;
    }

    auto _getGithubLatestReleaseFrom(const char * repositoryOwnerName, const char * repositoryName) const {
        auto _remoteManifestURL = std::string("https://api.github.com/repos/") + std::string(repositoryOwnerName) + "/" + std::string(repositoryName) + "/releases/latest";
        spdlog::info("[Github] UpdateChecker : Downloading remote releases manifest [{}]", _remoteManifestURL);
        return Downloader::dumbGet(_remoteManifestURL).messageBody;
    }

    // returns if successfully requested updater to run
    static bool tryToLaunchUpdater(const std::filesystem::path &updaterPath = _expectedMaintenanceToolPath()) {
        if (!std::filesystem::exists(updaterPath)) {
            spdlog::warn("[IFW] UpdateChecker : Cannot find updater at [{}], aborting", updaterPath.string());
            return false;
        }

        // run
        #if defined UNICODE && defined _WIN32
            std::vector<TinyProcessLib::Process::string_type> args {updaterPath.wstring(), L"--updater"};
        #else
            std::vector<TinyProcessLib::Process::string_type> args {updaterPath.string(), "--updater"};
        #endif

        spdlog::info("[IFW] UpdateChecker : Launching updater [{}] ...", updaterPath.string());
        
            TinyProcessLib::Process run(args);

        spdlog::info("UpdateChecker : Quitting ...");

        return true;
    }

 private:
    const std::string _remoteManifestURL;

    const CheckResults _gh_isNewerVersionAvailable() const {
        //
        if (strlen(GITHUB_REPO_OWNER) == 0 || strlen(GITHUB_REPO_NAME) == 0) {
            spdlog::info("[Github] UpdateChecker : GITHUB_REPO_OWNER or GITHUB_REPO_NAME not configured, cannot check updates against Github official repository.");
        }

        auto ghLatestRelease = _getGithubLatestReleaseFrom(GITHUB_REPO_OWNER, GITHUB_REPO_NAME);

        // if no remoteManifest given, skip
        if(ghLatestRelease.empty()) {
            spdlog::warn("[Github] UpdateChecker : could not download remote manifest !");
            return {CheckSource::GithubCheck, CheckCode::RemoteManifestFetch };
        }

        auto ghRemote = _extractTagNameFromGithubReleaseManifest(ghLatestRelease);
        
        // if no remoteManifest given, skip
        if(!ghRemote.size()) {
            spdlog::warn("[Github] UpdateChecker : no remote manifest version found !");
            return {CheckSource::GithubCheck, CheckCode::RemoteManifestRead };
        }

        //
        auto &remoteVersion = ghRemote.begin()->second;
        spdlog::info("[Github] UpdateChecker : remote version {}", remoteVersion);

        // compare versions
        auto isNewer = _isRemoteVersionNewerThanLocal(APP_CURRENT_VERSION, remoteVersion);
        if (isNewer) {
            spdlog::info("[Github] UpdateChecker : Local version [{}] older than remote [{}]", APP_CURRENT_VERSION, remoteVersion);
        } else {
            spdlog::info("[Github] UpdateChecker : No components to be updated");
        }

        return {CheckSource::GithubCheck, CheckCode::Succeeded, isNewer};
    }

    const CheckResults _isNewerVersionAvailable() const {
        //
        spdlog::info("UpdateChecker : local version is \"{}\"", APP_CURRENT_VERSION);

        //
        //

        //
        spdlog::info("UpdateChecker : Checking updates...");

        //
        auto ghCheck = _gh_isNewerVersionAvailable();
        if (ghCheck.result == CheckCode::Succeeded) {
            return ghCheck;
        }

        //
        //

        // if no remoteManifest given, skip
        if(_remoteManifestURL.empty()) {
            spdlog::warn("[IFW] UpdateChecker : no remote manifest url configured !");
            return {CheckSource::IFWCheck, CheckCode::NoRemoteURL };
        }

        // fetch local
        auto localRaw = _getLocalManifestContent();
        if (localRaw.empty()) {
            _manifestFetchingFailed("local");
            return {CheckSource::IFWCheck, CheckCode::LocalManifestFetch };
        }

        auto localComponents = _extractVersionsFromManifest(localRaw);
        if (!localComponents.size()) {
            _manifestFetchingFailed("local");
            return {CheckSource::IFWCheck, CheckCode::LocalManifestRead };
        }

        // fetch remote
        auto remoteRaw = _getRemoteManifestContent();
        if (remoteRaw.empty()) {
            _manifestFetchingFailed("remote");
            return {CheckSource::IFWCheck, CheckCode::RemoteManifestFetch };
        }

        auto remoteVersions = _extractVersionsFromManifest(remoteRaw);
        if (!remoteVersions.size()) {
            _manifestFetchingFailed("remote");
            return {CheckSource::IFWCheck, CheckCode::RemoteManifestRead };
        }

        // iterate through local components
        for (auto &[component, localVersion] : localComponents) {
            // if local component not found on remote, needs update
            auto foundOnRemote = remoteVersions.find(component);
            if (foundOnRemote == remoteVersions.end()) {
                spdlog::info("[IFW] UpdateChecker : Local component [{}] not found on remote", component);
                return {CheckSource::IFWCheck, CheckCode::Succeeded, true};
            }

            // compare versions
            auto &remoteVersion = foundOnRemote->second;
            auto isNewer = _isRemoteVersionNewerThanLocal(localVersion, remoteVersion);
            if (isNewer) {
                spdlog::info("[IFW] UpdateChecker : Local component [{} : {}] older than remote [{}]", component, localVersion, remoteVersion);
                return {CheckSource::IFWCheck, CheckCode::Succeeded, true};
            }

            // if not older, remove from remote
            spdlog::info("[IFW] UpdateChecker : Local component [{}] up-to-date", component);
            remoteVersions.erase(component);
        }

        // if any components remaining in remote, has updates
        if (remoteVersions.size()) {
            auto &firstComponent = remoteVersions.begin()->first;
            spdlog::info("[IFW] UpdateChecker : Remote component [{}] not found in local", firstComponent);
            return {CheckSource::IFWCheck, CheckCode::Succeeded, true};
        }

        //
        spdlog::info("[IFW] UpdateChecker : No components to be updated");
        return {CheckSource::IFWCheck, CheckCode::Succeeded, false};
    }

    static void _manifestFetchingFailed(const char* manifestType) {
        spdlog::warn("[IFW] UpdateChecker : Error while fetching {} manifest !", manifestType);
    }
};
