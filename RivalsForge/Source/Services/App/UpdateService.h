#pragma once
#include <functional>
#include <string>

// The manager's self-update (the Inazuma Eleven VR Save Editor's UpdateService): version.txt and
// changelog.txt from the GitHub repository (Config::GitHub), the release zip RivalsForgev<x-y-z>.zip
// (automatic update) or the bare exe (manual update) from its latest release.
// Every function blocks: call them from worker threads.
namespace RivalsForge::Services::Updates {

struct UpdateInfo {
    std::wstring latestVersion;
    std::wstring changelog;
    std::wstring downloadUrl;
    std::wstring fileName;
    long long fileSize = 0;
    bool isUpdateAvailable = false;
    std::wstring releaseUrl;
};

// Failures are swallowed: the result then says "no update" (checked tells whether GitHub answered)
UpdateInfo CheckForUpdates(bool* checked = nullptr);
// The manual update: the latest release's exe whatever the version comparison says (an empty
// downloadUrl when GitHub couldn't be reached)
UpdateInfo LatestReleaseInfo();
// The live changelog.txt from GitHub, or the embedded one when the fetch fails / is empty
std::wstring FetchChangelog();

using Progress = std::function<void(int percent)>;
// Downloads the release's zip (taking the exe out of it) or the bare exe, then a helper batch waits
// for the manager to close, swaps the exe and opens it again; exits the process. Returns false (and
// keeps running) when the download fails.
bool DownloadAndInstall(const UpdateInfo& info, const Progress& progress);

std::wstring RepoUrl();
std::wstring ReleasesUrl();   // the latest release's page

} // namespace RivalsForge::Services::Updates
