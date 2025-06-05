#include "update.h"
#include <imgui.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <memory>
#include <array>
#include <algorithm> 
#include <vector>

#ifdef _WIN32
    #include <windows.h>
    #include <shellapi.h>
    #include <winhttp.h>
    #pragma comment(lib, "winhttp.lib")
#elif __APPLE__
    #include <CoreFoundation/CoreFoundation.h>
    #include <ApplicationServices/ApplicationServices.h>
    #include <unistd.h>
    #include <sys/wait.h>
#endif

#include <nlohmann/json.hpp>

struct UpdateChecker::Impl {
    std::atomic<bool> requestInProgress{false};
    std::atomic<bool> updateCheckComplete{false};
    std::mutex dataMutex;
    
    std::string latestVersionFound;
    std::string downloadUrlFound;
    bool updateFoundFlag = false;
};

UpdateChecker::UpdateChecker() : pImpl(std::make_unique<Impl>()) {}
UpdateChecker::~UpdateChecker() {}

bool UpdateChecker::isSupportedPlatform() const {
#ifdef _WIN32
    return true;
#elif __APPLE__
    #ifdef __aarch64__
    return true;
    #else
    return false;
    #endif
#else
    return false;
#endif
}

std::string UpdateChecker::getPlatformString() const {
#ifdef _WIN32
    return "windows";
#elif __APPLE__
    return "macos";
#else
    return "unknown";
#endif
}

bool UpdateChecker::isNewerVersion(const std::string& current, const std::string& latest) const {
    auto parseVersion = [](const std::string& version) {
        std::vector<int> parts;
        std::stringstream ss(version);
        std::string part;
        
        // Remove 'v' prefix if present
        std::string cleanVersion = version;
        if (!cleanVersion.empty() && cleanVersion[0] == 'v') {
            cleanVersion = cleanVersion.substr(1);
        }
        
        std::stringstream cleanSs(cleanVersion);
        while (std::getline(cleanSs, part, '.')) {
            try {
                parts.push_back(std::stoi(part));
            } catch (...) {
                parts.push_back(0);
            }
        }
        
        // Ensure we have at least 3 parts
        while (parts.size() < 3) {
            parts.push_back(0);
        }
        
        return parts;
    };
    
    auto currentParts = parseVersion(current);
    auto latestParts = parseVersion(latest);
    
    size_t minSize = (currentParts.size() < latestParts.size()) ? currentParts.size() : latestParts.size();
    for (size_t i = 0; i < minSize; ++i) {
        if (latestParts[i] > currentParts[i]) {
            return true;
        } else if (latestParts[i] < currentParts[i]) {
            return false;
        }
    }
    
    return false;
}

void UpdateChecker::checkForUpdates(const std::string& repoOwner, const std::string& repoName) {
    if (!isSupportedPlatform() || pImpl->requestInProgress.load()) {
        return;
    }
    
    pImpl->requestInProgress = true;
    pImpl->updateCheckComplete = false;
    
    std::string apiUrl = "https://api.github.com/repos/" + repoOwner + "/" + repoName + "/releases/latest";
    
    // Perform HTTP request in a separate thread
    std::thread([this, apiUrl]() {
        performHttpRequest(apiUrl, [this](const std::string& response) {
            std::lock_guard<std::mutex> lock(pImpl->dataMutex);
            
            try {
                auto json = nlohmann::json::parse(response);
                
                if (json.contains("tag_name") && json.contains("html_url")) {
                    std::string latestVersion = json["tag_name"];
                    std::string htmlUrl = json["html_url"];
                    
                    pImpl->latestVersionFound = latestVersion;
                    pImpl->downloadUrlFound = htmlUrl;
                    pImpl->updateFoundFlag = true;
                }
            } catch (const std::exception& e) {
                // JSON parsing failed - ignore silently
                pImpl->updateFoundFlag = false;
            }
            
            pImpl->updateCheckComplete = true;
            pImpl->requestInProgress = false;
        });
    }).detach();
}

// Helper function to execute command and capture output
std::string executeCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    
#ifdef _WIN32
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
#endif
    
    if (!pipe) {
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

void UpdateChecker::performHttpRequest(const std::string& url, std::function<void(const std::string&)> callback) {
#ifdef _WIN32
    // Windows implementation using WinHTTP
    HINTERNET hSession = WinHttpOpen(L"UpdateChecker/1.0", 
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, 
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (!hSession) {
        callback("");
        return;
    }
    
    // Parse URL
    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    
    wchar_t hostname[256] = {0};
    wchar_t path[1024] = {0};
    
    urlComp.lpszHostName = hostname;
    urlComp.dwHostNameLength = sizeof(hostname)/sizeof(wchar_t);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path)/sizeof(wchar_t);
    
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        WinHttpCloseHandle(hSession);
        callback("");
        return;
    }
    
    HINTERNET hConnect = WinHttpConnect(hSession, hostname, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        callback("");
        return;
    }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL, 
                                           WINHTTP_NO_REFERER, 
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        callback("");
        return;
    }
    
    // Send request
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {
        
        std::string response;
        DWORD bytesAvailable = 0;
        
        do {
            if (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
                std::vector<char> buffer(bytesAvailable + 1);
                DWORD bytesRead = 0;
                
                if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
                    response.append(buffer.data(), bytesRead);
                }
            }
        } while (bytesAvailable > 0);
        
        callback(response);
    } else {
        callback("");
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
#elif __APPLE__
    std::string command = "curl -s -L --max-time 10 \"" + url + "\"";
    std::string response = executeCommand(command.c_str());
    callback(response);
#endif
}

void UpdateChecker::update(UpdateState& state) {
    if (pImpl->updateCheckComplete.load()) {
        std::lock_guard<std::mutex> lock(pImpl->dataMutex);
        
        if (pImpl->updateFoundFlag) {
            bool isNewer = isNewerVersion(state.currentVersion, pImpl->latestVersionFound);
            
            if (isNewer) {
                state.updateAvailable = true;
                state.latestVersion = pImpl->latestVersionFound;
                state.downloadUrl = pImpl->downloadUrlFound;
                state.shouldShowBanner = true;
                state.updatePromptVisible = true;
            }
        }
        
        pImpl->updateCheckComplete = false;
        pImpl->updateFoundFlag = false;
    }
    
    state.checkingForUpdate = pImpl->requestInProgress.load();
}

bool UpdateChecker::shouldShowUpdateBanner(const UpdateState& state) const {
    return state.shouldShowBanner && state.updatePromptVisible && state.updateAvailable;
}

void UpdateChecker::drawUpdateBanner(UpdateState& state, float windowWidth, float sidebarWidth) {
    if (!shouldShowUpdateBanner(state)) {
        return;
    }
    
    float bannerWidth = windowWidth - sidebarWidth;
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(bannerWidth, state.bannerHeight));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.00f, 0.00f, 0.00f, 0.0f));
    
    ImGui::Begin("##UpdateBanner", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse);
    
    float textHeight = ImGui::GetTextLineHeight();
    float centerY = (state.bannerHeight - textHeight) * 0.5f;
    
    ImGui::SetCursorPosY(centerY);
    ImGui::Text("There's an update available,");
    ImGui::SameLine();

    ImGui::SetCursorPosY(centerY - (ImGui::GetFrameHeight() - textHeight) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
    if (ImGui::Button("download it?")) {
        openDownloadUrl(state.downloadUrl);
    }
    ImGui::PopStyleColor();

    ImVec2 buttonMin = ImGui::GetItemRectMin();
    ImVec2 buttonMax = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(buttonMin.x, buttonMax.y - 1), 
        ImVec2(buttonMax.x, buttonMax.y - 1),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.8f, 1.0f, 1.0f)), 
        1.0f
    );
    
    ImGui::SameLine();
    float closeButtonX = bannerWidth - 20 - ImGui::GetStyle().WindowPadding.x;
    ImGui::SetCursorPosX(closeButtonX);
    
    float buttonCenterY = (state.bannerHeight - 16) * 0.5f;
    ImGui::SetCursorPosY(buttonCenterY);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    
    if (ImGui::Button("X##close", ImVec2(16, 16))) {
        state.updatePromptVisible = false;
        state.shouldShowBanner = false;
    }
    
    ImGui::PopStyleVar();
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void UpdateChecker::openDownloadUrl(const std::string& url) {
#ifdef _WIN32
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif __APPLE__
    std::string command = "open \"" + url + "\"";
    system(command.c_str());
#endif
}
