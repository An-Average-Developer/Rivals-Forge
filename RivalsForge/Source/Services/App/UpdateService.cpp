#include "Services/App/UpdateService.h"
#include <windows.h>
#include <shellapi.h>
#include <optional>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Services/App/ChangelogData.h"
#include "Services/App/Log.h"
#include "Services/App/VersionCompare.h"

namespace RivalsForge::Services::Updates {

namespace {

std::wstring RawUrl(const wchar_t* file) {
    return std::wstring(L"https://raw.githubusercontent.com/") + Config::GitHub::Owner + L"/" + Config::GitHub::Repo + L"/" +
           Config::GitHub::Branch + L"/" + file;
}

// RivalsForgev1-0-1.zip for 1.0.1
std::wstring ZipName(const std::wstring& version) { return Config::GitHub::ZipPrefix + Text::Replace(version, L".", L"-") + L".zip"; }

// version.txt without \r \n \t, trimmed; nullopt on a network error, an empty file or anything that
// isn't a version number (X.Y...: a 404 page or an error body)
std::optional<std::wstring> FetchVersion() {
    const Http::Response r = Http::Get(RawUrl(L"version.txt"), {}, 15000);
    if (!r.Ok()) return std::nullopt;
    std::wstring v;
    for (wchar_t c : Text::Widen(r.body))
        if (c != L'\r' && c != L'\n' && c != L'\t') v += c;
    v = Text::Trim(v);
    std::wstring digits;
    for (wchar_t c : v)
        if ((c >= L'0' && c <= L'9') || c == L'.') digits += c;
    const auto parts = Text::Split(digits, L'.', true);
    if (parts.size() < 2) return std::nullopt;
    for (const auto& p : parts)
        if (!Text::ToInt(p)) return std::nullopt;
    return v;
}

long long HeadContentLength(const std::wstring& url) {
    Http::Request head;
    head.method = L"HEAD";
    head.url = url;
    head.timeoutMs = 15000;
    const Http::Response r = Http::Send(head);
    return r.Ok() ? Text::ToInt(r.Header(L"content-length")).value_or(0) : 0;
}

// Batch lines that wait (up to ~30 s) until this process is gone, then about one second more (ping
// waits: timeout fails without a console; full paths: a Git / MSYS folder on PATH brings its own find)
std::wstring WaitForExitLines() {
    const std::wstring pid = std::to_wstring(GetCurrentProcessId());
    return L"set WAITS=30\r\n:wait\r\n"
           L"\"%SystemRoot%\\System32\\tasklist.exe\" /FI \"PID eq " + pid + L"\" /NH 2>nul | \"%SystemRoot%\\System32\\find.exe\" /I \".exe\" >nul\r\n"
           L"if not errorlevel 1 (\r\n    set /a WAITS-=1\r\n    if %WAITS% GTR 0 (\r\n"
           L"        ping -n 2 127.0.0.1 >nul\r\n        goto wait\r\n    )\r\n)\r\n"
           L"ping -n 2 127.0.0.1 >nul\r\n";
}

// A Windows program, not an error page saved as the exe
bool LooksLikeExe(const std::wstring& path) {
    const auto bytes = Files::ReadBytes(path);
    return bytes && bytes->size() > 64 * 1024 && (*bytes)[0] == 'M' && (*bytes)[1] == 'Z';
}

} // namespace

std::wstring RepoUrl() { return std::wstring(L"https://github.com/") + Config::GitHub::Owner + L"/" + Config::GitHub::Repo; }

std::wstring ReleasesUrl() { return RepoUrl() + L"/releases/latest"; }

UpdateInfo CheckForUpdates(bool* checked) {
    UpdateInfo info;
    info.latestVersion = Versions::Current();
    if (checked) *checked = false;
    const auto latest = FetchVersion();
    if (!latest) return info;
    if (checked) *checked = true;
    info.latestVersion = *latest;
    info.changelog = FetchChangelog();
    info.isUpdateAvailable = Versions::IsNewer(Versions::Current(), *latest);
    info.releaseUrl = ReleasesUrl();
    if (info.isUpdateAvailable) {
        info.fileName = ZipName(*latest);
        info.downloadUrl = ReleasesUrl() + L"/download/" + info.fileName;
        info.fileSize = HeadContentLength(info.downloadUrl);
    }
    return info;
}

UpdateInfo LatestReleaseInfo() {
    UpdateInfo info;
    info.latestVersion = Versions::Current();
    const auto latest = FetchVersion();
    if (!latest) return info;
    info.latestVersion = *latest;
    info.isUpdateAvailable = true;
    info.changelog = FetchChangelog();
    info.fileName = Config::GitHub::ExeAsset;
    info.downloadUrl = Config::GitHub::LatestExeUrl;
    info.releaseUrl = ReleasesUrl();
    info.fileSize = HeadContentLength(info.downloadUrl);
    return info;
}

std::wstring FetchChangelog() {
    const Http::Response r = Http::Get(RawUrl(L"changelog.txt"), {}, 15000);
    if (r.Ok()) {
        const std::wstring changelog = Text::Trim(Text::Widen(r.body));
        if (!changelog.empty()) return changelog;
    }
    return Changelog::EmbeddedText();
}

bool DownloadAndInstall(const UpdateInfo& info, const Progress& progress) {
    if (Text::Trim(info.downloadUrl).empty()) return false;
    const std::wstring tempDir = Files::Combine(Files::TempDirectory(), Config::UpdateFolder);
    Files::DeleteDirectory(tempDir);
    Files::CreateDirectories(tempDir);
    // The release zip (automatic update) or the bare exe (manual update)
    const bool zip = Text::EndsWith(info.downloadUrl, L".zip", true);
    const std::wstring downloadPath = Files::Combine(tempDir, zip ? info.fileName : std::wstring(L"download.exe"));
    const std::wstring staged = Files::Combine(tempDir, Config::GitHub::ExeAsset);
    Log::Info(L"Update: downloading " + info.downloadUrl + L" (" + info.latestVersion + L")");
    std::wstring error;
    const bool downloaded = Http::Download(info.downloadUrl, downloadPath, [&](uint64_t received, uint64_t total) {
        if (progress && total > 0) progress(static_cast<int>(received * 100 / total));
        return true;
    }, &error);
    const auto fail = [&](const std::wstring& why) {
        Log::Error(L"Update failed: " + why);
        Files::DeleteDirectory(tempDir);
        return false;
    };
    if (!downloaded) return fail(error);
    if (zip) {
        // RivalsForge.exe out of the zip (any exe when it's named otherwise)
        const auto reader = Zip::Reader::Open(downloadPath);
        if (!reader) return fail(L"the zip could not be opened");
        const Zip::Entry* exe = nullptr;
        for (const auto& entry : reader->Entries()) {
            if (Text::Equals(entry.name, Config::GitHub::ExeAsset, true)) {
                exe = &entry;
                break;
            }
            if (!exe && Text::EndsWith(entry.name, L".exe", true)) exe = &entry;
        }
        if (!exe) return fail(L"no exe in the zip");
        if (!reader->ExtractToFile(*exe, staged)) return fail(L"the exe could not be extracted");
    } else if (!Files::Move(downloadPath, staged)) {
        return fail(L"the download could not be moved");
    }
    if (!LooksLikeExe(staged)) return fail(L"the download is not a program");
    if (progress) progress(100);

    // The copy's output goes to update.log; on failure the temp folder stays for diagnosis
    // ("Sharing violation" = the exe was still locked, retried meanwhile)
    const std::wstring currentExe = Files::ExecutablePath();
    const std::wstring log = Files::Combine(tempDir, L"update.log");
    const std::string batch = Text::Narrow(
        L"@echo off\r\nchcp 65001 >nul\r\n" + WaitForExitLines() +
        L"set RETRIES=15\r\n:copy\r\n"
        L"copy /y \"" + staged + L"\" \"" + currentExe + L"\" >> \"" + log + L"\" 2>&1\r\n"
        L"if not errorlevel 1 goto copied\r\n"
        L"set /a RETRIES-=1\r\nif %RETRIES% LEQ 0 exit /b 1\r\n"
        L"ping -n 2 127.0.0.1 >nul\r\ngoto copy\r\n:copied\r\n"
        L"start \"\" \"" + currentExe + L"\"\r\n"
        L"rd /s /q \"" + tempDir + L"\"\r\nexit\r\n");
    const std::wstring batchPath = Files::Combine(tempDir, L"update.bat");
    if (!Files::WriteBytes(batchPath, batch)) return false;
    Log::Info(L"Update: downloaded, closing to let update.bat replace the exe");
    SHELLEXECUTEINFOW run{ sizeof(run) };
    run.lpVerb = L"open";
    run.lpFile = L"cmd.exe";
    const std::wstring args = L"/c \"" + batchPath + L"\"";
    run.lpParameters = args.c_str();
    run.nShow = SW_HIDE;
    if (!ShellExecuteExW(&run)) {
        Log::Error(L"Update failed: update.bat could not be started");
        return false;
    }
    ExitProcess(0);   // the batch waits for this process to be gone
}

} // namespace RivalsForge::Services::Updates
