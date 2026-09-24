#include "updater.hpp"
#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

static std::mutex g_ini_mutex;

static void update_ini_key(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(g_ini_mutex);
    std::string path = get_vibe_dir() + "/state.ini";
    std::ifstream in(path);
    std::vector<std::pair<std::string, std::string>> entries;
    bool found = false;

    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                if (k == key) {
                    if (!value.empty()) {
                        entries.push_back({k, value});
                    }
                    found = true;
                } else {
                    entries.push_back({k, v});
                }
            }
        }
        in.close();
    }

    if (!found && !value.empty()) {
        entries.push_back({key, value});
    }

    std::ofstream out(path);
    if (out.is_open()) {
        for (const auto& entry : entries) {
            out << entry.first << "=" << entry.second << "\n";
        }
    }
}

static std::string read_ini_key(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_ini_mutex);
    std::string path = get_vibe_dir() + "/state.ini";
    std::ifstream in(path);
    if (!in.is_open()) {
        const char* home = getenv("HOME");
        if (home) {
            in.open(std::string(home) + "/.vibe-fi-state.ini");
        }
    }
    if (!in.is_open()) return "";

    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            if (k == key) return v;
        }
    }
    return "";
}

std::string check_latest_version(int timeout_seconds) {
    std::string curl_path = find_executable("curl");
    if (curl_path.empty()) {
        return "";
    }

    std::string url = "https://api.github.com/repos/Swadesh-c0de/vibe-fi/releases/latest";
    std::string cmd = shell_escape(curl_path) + " -s --max-time " + std::to_string(timeout_seconds) +
                      " -H \"User-Agent: vibe-fi\" " + shell_escape(url) + " 2>/dev/null";

    UniquePipe pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        return "";
    }

    std::string response;
    std::array<char, 1024> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        response += buffer.data();
    }

    // Parse "tag_name" field from GitHub JSON
    size_t tag_pos = response.find("\"tag_name\"");
    if (tag_pos == std::string::npos) {
        return "";
    }

    size_t colon_pos = response.find(':', tag_pos);
    if (colon_pos == std::string::npos) {
        return "";
    }

    size_t first_quote = response.find('"', colon_pos);
    if (first_quote == std::string::npos) {
        return "";
    }

    size_t second_quote = response.find('"', first_quote + 1);
    if (second_quote == std::string::npos) {
        return "";
    }

    return response.substr(first_quote + 1, second_quote - first_quote - 1);
}

static std::vector<int> parse_semver(const std::string& v) {
    std::vector<int> nums;
    std::string clean = v;
    if (!clean.empty() && (clean[0] == 'v' || clean[0] == 'V')) {
        clean = clean.substr(1);
    }
    std::stringstream ss(clean);
    std::string item;
    while (std::getline(ss, item, '.')) {
        std::string digits;
        for (char c : item) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                digits += c;
            } else {
                break;
            }
        }
        if (!digits.empty()) {
            nums.push_back(std::stoi(digits));
        } else {
            nums.push_back(0);
        }
    }
    while (nums.size() < 3) {
        nums.push_back(0);
    }
    return nums;
}

bool is_newer_version(const std::string& latest, const std::string& current) {
    if (latest.empty()) return false;
    auto l = parse_semver(latest);
    auto c = parse_semver(current);
    for (size_t i = 0; i < 3; ++i) {
        if (l[i] > c[i]) return true;
        if (l[i] < c[i]) return false;
    }
    return false;
}

std::string get_current_executable_path(const char* argv0) {
#if defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        std::error_code ec;
        fs::path p = fs::canonical(buf, ec);
        if (!ec) return p.string();
        return std::string(buf);
    }
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::error_code ec;
        fs::path p = fs::canonical(buf, ec);
        if (!ec) return p.string();
        return std::string(buf);
    }
#endif

    // Fallback using argv0 or PATH lookup
    if (argv0 != nullptr && argv0[0] != '\0') {
        std::error_code ec;
        fs::path p = fs::canonical(argv0, ec);
        if (!ec && fs::exists(p)) return p.string();
    }

    std::string in_path = find_executable("vibe");
    if (!in_path.empty()) return in_path;

    return argv0 ? argv0 : "vibe";
}

bool perform_update(const std::string& latest_version, const std::string& current_exe_path) {
    std::cout << "\n\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "  \033[1;37mUpdating Vibe-Fi to " << latest_version << "...\033[0m\n";
    std::cout << "\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n\n";

    std::string git = find_executable("git");
    std::string cmake = find_executable("cmake");
    if (git.empty() || cmake.empty()) {
        std::cerr << ":: Error: git and cmake are required to perform an update.\n";
        return false;
    }

    fs::path temp_dir = fs::temp_directory_path() / "vibe-fi-update";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    std::cout << ":: Fetching latest repository source (" << latest_version << ")...\n";
    std::string clone_cmd = "git clone --depth 1 https://github.com/Swadesh-c0de/vibe-fi.git " + shell_escape(temp_dir.string());
    int ret = std::system(clone_cmd.c_str());
    if (ret != 0) {
        std::cerr << ":: Error: Failed to clone repository.\n";
        fs::remove_all(temp_dir, ec);
        return false;
    }

    std::cout << ":: Configuring Release build...\n";
    std::string build_dir = (temp_dir / "build").string();
    std::string cfg_cmd = "cmake -B " + shell_escape(build_dir) +
                          " -DCMAKE_BUILD_TYPE=Release -S " + shell_escape(temp_dir.string()) + " >/dev/null";
    ret = std::system(cfg_cmd.c_str());
    if (ret != 0) {
        std::cerr << ":: Error: CMake build configuration failed.\n";
        fs::remove_all(temp_dir, ec);
        return false;
    }

    std::cout << ":: Compiling Vibe-Fi binary...\n";
    std::string build_cmd = "cmake --build " + shell_escape(build_dir) + " -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2) >/dev/null";
    ret = std::system(build_cmd.c_str());
    if (ret != 0) {
        std::cerr << ":: Error: Compilation failed.\n";
        fs::remove_all(temp_dir, ec);
        return false;
    }

    fs::path built_bin = temp_dir / "build" / "vibe_fi";
    if (!fs::exists(built_bin)) {
        std::cerr << ":: Error: Compiled binary not found.\n";
        fs::remove_all(temp_dir, ec);
        return false;
    }

    std::cout << ":: Installing updated binary...\n";
    std::vector<std::string> target_paths;
    if (!current_exe_path.empty()) {
        target_paths.push_back(current_exe_path);
    }

    char* home = std::getenv("HOME");
    if (home) {
        std::string user_vibe = std::string(home) + "/.local/bin/vibe";
        if (user_vibe != current_exe_path) {
            target_paths.push_back(user_vibe);
        }
    }

    bool install_success = false;
    for (const auto& target : target_paths) {
        fs::path target_p(target);
        fs::create_directories(target_p.parent_path(), ec);

        std::string install_cmd;
        if (access(target_p.parent_path().c_str(), W_OK) == 0 &&
            (!fs::exists(target_p) || access(target.c_str(), W_OK) == 0)) {
            install_cmd = "install -m 755 " + shell_escape(built_bin.string()) + " " + shell_escape(target);
        } else {
            std::cout << ":: Administrator permission required for: " << target << "...\n";
            install_cmd = "sudo install -m 755 " + shell_escape(built_bin.string()) + " " + shell_escape(target);
        }

        int inst_ret = std::system(install_cmd.c_str());
        if (inst_ret == 0) {
            install_success = true;
        }
    }

    fs::remove_all(temp_dir, ec);
    return install_success;
}

bool prompt_and_handle_update(int argc, char* argv[], bool force_check) {
    (void)argc;
    if (!force_check && !isatty(fileno(stdin))) {
        return false;
    }

    std::string latest = check_latest_version(2);
    if (latest.empty()) {
        if (force_check) {
            std::cout << ":: Could not fetch latest release info from GitHub (offline or rate-limited).\n";
        }
        return false;
    }

    std::string current = VIBE_FI_VERSION;
    if (!is_newer_version(latest, current)) {
        if (force_check) {
            std::cout << ":: Vibe-Fi is up to date (" << current << ").\n";
        }
        return false;
    }

    // Display update notification banner
    std::cout << "\n\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "  \033[1;37mA new version of Vibe-Fi is available\033[0m\n";
    std::cout << "  Current: \033[0;31m" << current << "\033[0m  ->  Latest: \033[1;32m" << latest << "\033[0m\n";
    std::cout << "\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "Do you want to update now? [\033[1;32my\033[0m/\033[1;31mN\033[0m]: ";
    std::cout.flush();

    std::string response;
    if (!std::getline(std::cin, response)) {
        return false;
    }

    // Trim whitespace
    while (!response.empty() && (response.front() == ' ' || response.front() == '\t')) {
        response.erase(response.begin());
    }
    while (!response.empty() && (response.back() == ' ' || response.back() == '\t' || response.back() == '\r')) {
        response.pop_back();
    }

    if (response == "y" || response == "Y" || response == "yes" || response == "Yes" || response == "YES") {
        std::string current_exe = get_current_executable_path(argv[0]);
        bool ok = perform_update(latest, current_exe);
        if (ok) {
            update_ini_key("available_update", "");
            update_ini_key("update_dismissed", "");
            std::cout << ":: Update completed successfully.\n";
            std::cout << ":: Resuming Vibe-Fi (" << latest << ")...\n\n";
            execvp(current_exe.c_str(), argv);
            execvp(argv[0], argv);
            return true;
        } else {
            std::cout << ":: Update failed. Continuing with existing version...\n\n";
            return false;
        }
    } else {
        std::cout << "-> Skipping update for now. Starting Vibe-Fi...\n\n";
        return false;
    }
}

bool check_and_prompt_cached_update(int argc, char* argv[]) {
    (void)argc;
    if (!isatty(fileno(stdin))) {
        return false;
    }

    std::string available = read_ini_key("available_update");
    if (available.empty()) {
        return false;
    }

    std::string current = VIBE_FI_VERSION;
    if (!is_newer_version(available, current)) {
        update_ini_key("available_update", "");
        return false;
    }

    std::string dismissed = read_ini_key("update_dismissed");
    if (dismissed == available) {
        return false;
    }

    // Display instant update notification banner (0ms network delay!)
    std::cout << "\n\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "  \033[1;37mA new version of Vibe-Fi is available\033[0m\n";
    std::cout << "  Current: \033[0;31m" << current << "\033[0m  ->  Latest: \033[1;32m" << available << "\033[0m\n";
    std::cout << "\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "Do you want to update now? [\033[1;32my\033[0m/\033[1;31mN\033[0m]: ";
    std::cout.flush();

    std::string response;
    if (!std::getline(std::cin, response)) {
        return false;
    }

    // Trim whitespace
    while (!response.empty() && (response.front() == ' ' || response.front() == '\t')) {
        response.erase(response.begin());
    }
    while (!response.empty() && (response.back() == ' ' || response.back() == '\t' || response.back() == '\r')) {
        response.pop_back();
    }

    if (response == "y" || response == "Y" || response == "yes" || response == "Yes" || response == "YES") {
        std::string current_exe = get_current_executable_path(argv[0]);
        bool ok = perform_update(available, current_exe);
        if (ok) {
            update_ini_key("available_update", "");
            update_ini_key("update_dismissed", "");
            std::cout << ":: Update completed successfully.\n";
            std::cout << ":: Resuming Vibe-Fi (" << available << ")...\n\n";
            execvp(current_exe.c_str(), argv);
            execvp(argv[0], argv);
            return true;
        } else {
            std::cout << ":: Update failed. Continuing with existing version...\n\n";
            return false;
        }
    } else {
        update_ini_key("update_dismissed", available);
        std::cout << "-> Skipping update for now. Starting Vibe-Fi...\n\n";
        return false;
    }
}

static std::thread g_bg_update_thread;
static std::atomic<bool> g_bg_update_running{false};
static std::mutex g_update_cb_mutex;
static std::function<void(const std::string&)> g_on_update_found = nullptr;

void start_background_update_check(std::function<void(const std::string&)> on_update_found) {
    if (g_bg_update_running.exchange(true)) {
        return; // Already running
    }

    {
        std::lock_guard<std::mutex> lock(g_update_cb_mutex);
        g_on_update_found = on_update_found;
    }

    g_bg_update_thread = std::thread([]() {
        // Sleep 5 seconds to let UI and audio startup finish smoothly
        for (int i = 0; i < 50 && g_bg_update_running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!g_bg_update_running) return;

        // Rate limiting: check at most once per 24 hours (86400 seconds)
        std::string last_check_str = read_ini_key("last_update_check");
        int64_t last_check = safe_stoll(last_check_str, 0);
        auto now = std::chrono::system_clock::now();
        int64_t now_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        if (last_check > 0 && (now_sec - last_check) < 86400) {
            g_bg_update_running = false;
            return;
        }

        // Quick connectivity check before making network call
        if (!is_online(1000)) {
            g_bg_update_running = false;
            return;
        }

        std::string latest = check_latest_version(5);
        if (!g_bg_update_running) return;

        // Record check timestamp
        update_ini_key("last_update_check", std::to_string(now_sec));

        if (!latest.empty() && is_newer_version(latest, VIBE_FI_VERSION)) {
            update_ini_key("available_update", latest);
            std::lock_guard<std::mutex> lock(g_update_cb_mutex);
            if (g_on_update_found && g_bg_update_running) {
                g_on_update_found(latest);
            }
        }

        g_bg_update_running = false;
    });
}

void stop_background_update_check() {
    g_bg_update_running = false;
    {
        std::lock_guard<std::mutex> lock(g_update_cb_mutex);
        g_on_update_found = nullptr;
    }
    if (g_bg_update_thread.joinable()) {
        g_bg_update_thread.detach();
    }
}

bool handle_uninstall(const char* argv0) {
    std::cout << "\n\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "  \033[1;37mVibe-Fi Uninstaller\033[0m\n";
    std::cout << "\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "This will remove the Vibe-Fi executable from your system.\n\n";

    std::vector<std::string> candidate_paths;
    std::string current_exe = get_current_executable_path(argv0);
    if (!current_exe.empty()) {
        candidate_paths.push_back(current_exe);
    }

    const char* home = std::getenv("HOME");
    if (home) {
        candidate_paths.push_back(std::string(home) + "/.local/bin/vibe");
        candidate_paths.push_back(std::string(home) + "/.local/bin/vibe_fi");
    }
    candidate_paths.push_back("/usr/local/bin/vibe");
    candidate_paths.push_back("/usr/local/bin/vibe_fi");
    candidate_paths.push_back("/usr/bin/vibe");
    candidate_paths.push_back("/usr/bin/vibe_fi");

    std::vector<std::string> existing_binaries;
    for (const auto& path : candidate_paths) {
        std::error_code ec;
        if (fs::exists(path, ec) || fs::is_symlink(path, ec)) {
            bool dup = false;
            for (const auto& ex : existing_binaries) {
                if (path == ex) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                existing_binaries.push_back(path);
            }
        }
    }

    if (existing_binaries.empty()) {
        std::cout << ":: No installed Vibe-Fi binaries detected on standard system paths.\n\n";
    } else {
        std::cout << "Detected binary installation(s):\n";
        for (const auto& bin : existing_binaries) {
            std::cout << "  -> \033[0;36m" << bin << "\033[0m\n";
        }
        std::cout << "\n";
    }

    std::cout << "Are you sure you want to uninstall Vibe-Fi? [\033[1;32my\033[0m/\033[1;31mN\033[0m]: ";
    std::cout.flush();

    std::string response;
    if (!std::getline(std::cin, response)) {
        return false;
    }

    while (!response.empty() && (response.front() == ' ' || response.front() == '\t')) response.erase(response.begin());
    while (!response.empty() && (response.back() == ' ' || response.back() == '\t' || response.back() == '\r')) response.pop_back();

    if (response != "y" && response != "Y" && response != "yes" && response != "Yes" && response != "YES") {
        std::cout << "-> Uninstallation cancelled. No changes were made.\n\n";
        return false;
    }

    std::cout << "\nRemoving binaries...\n";
    for (const auto& bin : existing_binaries) {
        std::error_code ec;
        fs::path p(bin);
        if (access(p.c_str(), W_OK) == 0 && access(p.parent_path().c_str(), W_OK) == 0) {
            if (fs::remove(p, ec)) {
                std::cout << "  [removed] " << bin << "\n";
            } else {
                std::cout << "  [error] Failed to remove: " << bin << " (" << ec.message() << ")\n";
            }
        } else {
            std::cout << "  :: Administrator permission required for: " << bin << "...\n";
            std::string rm_cmd = "sudo rm -f " + shell_escape(bin);
            int ret = std::system(rm_cmd.c_str());
            if (ret == 0) {
                std::cout << "  [removed] " << bin << "\n";
            } else {
                std::cout << "  [error] Failed to remove " << bin << " via sudo.\n";
            }
        }
    }

    // Clean isolated bottle directory (~/.vibe-fi/bottle)
    std::string bottle_dir = get_bottle_dir();
    BottleManifest manifest = read_bottle_manifest();
    std::error_code bec;
    if (fs::exists(bottle_dir, bec)) {
        std::cout << "\nCleaning isolated bottle dependencies...\n";
        if (fs::remove_all(bottle_dir, bec)) {
            std::cout << "  [removed] " << bottle_dir << " (isolated dependencies cleaned)\n";
        }
    }

    // Handle tracked system packages with reverse dependency checks
    if (!manifest.installed_system_packages.empty()) {
        std::vector<std::string> removable_pkgs;
        std::cout << "\nChecking tracked system packages installed for Vibe-Fi...\n";
        for (const auto& pkg : manifest.installed_system_packages) {
            bool in_use = false;
            if (manifest.package_manager == "apt") {
                std::string check_cmd = "apt-cache rdepends --installed " + shell_escape(pkg) + " 2>/dev/null";
                UniquePipe pipe(popen(check_cmd.c_str(), "r"));
                if (pipe) {
                    char buf[512];
                    int count = 0;
                    while (fgets(buf, sizeof(buf), pipe.get())) {
                        std::string line(buf);
                        if (line.find('|') != std::string::npos || (line.find("  ") == 0 && line.find(pkg) == std::string::npos)) {
                            count++;
                        }
                    }
                    in_use = (count > 0);
                }
            } else if (manifest.package_manager == "pacman") {
                std::string check_cmd = "pacman -Qi " + shell_escape(pkg) + " 2>/dev/null | grep -E '^Required By\\s*:\\s*(None|none)' >/dev/null 2>&1";
                int ret = std::system(check_cmd.c_str());
                in_use = (ret != 0);
            } else if (manifest.package_manager == "dnf" || manifest.package_manager == "zypper") {
                std::string check_cmd = "rpm -q --whatrequires " + shell_escape(pkg) + " 2>/dev/null | grep -v 'no package requires' | grep -v 'is not installed' | grep '[^[:space:]]' >/dev/null 2>&1";
                int ret = std::system(check_cmd.c_str());
                in_use = (ret == 0);
            } else if (manifest.package_manager == "brew") {
                std::string check_cmd = "brew uses --installed " + shell_escape(pkg) + " 2>/dev/null";
                UniquePipe pipe(popen(check_cmd.c_str(), "r"));
                if (pipe) {
                    char buf[512];
                    if (fgets(buf, sizeof(buf), pipe.get())) {
                        std::string line(buf);
                        in_use = (!line.empty() && line.find_first_not_of(" \t\r\n") != std::string::npos);
                    }
                }
            }

            if (in_use) {
                std::cout << "  :: Dependency Guard: " << pkg << " is now required by other software on your system.\n";
                std::cout << "  [retained] " << pkg << " (skipping removal to prevent breaking other apps)\n";
            } else {
                removable_pkgs.push_back(pkg);
            }
        }

        if (!removable_pkgs.empty()) {
            std::cout << "\nThe following system package(s) were installed for Vibe-Fi and are not needed by other apps:\n";
            for (const auto& pkg : removable_pkgs) {
                std::cout << "  -> \033[0;36m" << pkg << "\033[0m\n";
            }
            std::cout << "Do you also want to remove these packages from your system? [\033[1;32my\033[0m/\033[1;31mN\033[0m]: ";
            std::cout.flush();

            std::string pkg_resp;
            if (std::getline(std::cin, pkg_resp)) {
                while (!pkg_resp.empty() && (pkg_resp.front() == ' ' || pkg_resp.front() == '\t')) pkg_resp.erase(pkg_resp.begin());
                while (!pkg_resp.empty() && (pkg_resp.back() == ' ' || pkg_resp.back() == '\t' || pkg_resp.back() == '\r')) pkg_resp.pop_back();

                if (pkg_resp == "y" || pkg_resp == "Y" || pkg_resp == "yes" || pkg_resp == "Yes") {
                    std::string pkg_list;
                    for (const auto& p : removable_pkgs) {
                        pkg_list += " " + shell_escape(p);
                    }
                    std::string rm_pkg_cmd;
                    if (manifest.package_manager == "brew") {
                        rm_pkg_cmd = "brew uninstall" + pkg_list;
                    } else if (manifest.package_manager == "apt") {
                        rm_pkg_cmd = "sudo apt-get remove -y" + pkg_list;
                    } else if (manifest.package_manager == "pacman") {
                        rm_pkg_cmd = "sudo pacman -R --noconfirm" + pkg_list;
                    } else if (manifest.package_manager == "dnf") {
                        rm_pkg_cmd = "sudo dnf remove -y" + pkg_list;
                    } else if (manifest.package_manager == "zypper") {
                        rm_pkg_cmd = "sudo zypper remove -y" + pkg_list;
                    }

                    if (!rm_pkg_cmd.empty()) {
                        std::cout << "Removing package(s)... (" << rm_pkg_cmd << ")\n";
                        int ret = std::system(rm_pkg_cmd.c_str());
                        if (ret == 0) {
                            std::cout << "  [removed] Tracked system packages uninstalled.\n";
                        } else {
                            std::cout << "  [error] Package removal command exited with status " << ret << "\n";
                        }
                    }
                } else {
                    std::cout << "  [retained] System packages preserved.\n";
                }
            }
        }
    }

    // Ask regarding data & playlists
    std::string vibe_dir = get_vibe_dir();
    std::error_code dir_ec;
    if (fs::exists(vibe_dir, dir_ec)) {
        std::cout << "\nDo you also want to remove your playlists and configuration (~/.vibe-fi)? [\033[1;32my\033[0m/\033[1;31mN\033[0m]: ";
        std::cout.flush();

        std::string dir_resp;
        if (std::getline(std::cin, dir_resp)) {
            while (!dir_resp.empty() && (dir_resp.front() == ' ' || dir_resp.front() == '\t')) dir_resp.erase(dir_resp.begin());
            while (!dir_resp.empty() && (dir_resp.back() == ' ' || dir_resp.back() == '\t' || dir_resp.back() == '\r')) dir_resp.pop_back();

            if (dir_resp == "y" || dir_resp == "Y" || dir_resp == "yes" || dir_resp == "Yes" || dir_resp == "YES") {
                fs::remove_all(vibe_dir, dir_ec);
                std::cout << "  [removed] " << vibe_dir << "\n";
            } else {
                std::cout << "  [retained] " << vibe_dir << " (playlists and settings preserved)\n";
            }
        }
    }

    std::cout << "\n\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n";
    std::cout << "  \033[1;32mVibe-Fi has been successfully uninstalled.\033[0m\n";
    std::cout << "\033[1;36m─────────────────────────────────────────────────────────────\033[0m\n\n";

    return true;
}

