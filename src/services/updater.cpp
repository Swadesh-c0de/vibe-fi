#include "updater.hpp"
#include "utils.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

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
    std::cout << "\n\033[1;36m=============================================================\033[0m\n";
    std::cout << "  \033[1;32m Updating Vibe-Fi to " << latest_version << "...\033[0m\n";
    std::cout << "\033[1;36m=============================================================\033[0m\n\n";

    std::string git = find_executable("git");
    std::string cmake = find_executable("cmake");
    if (git.empty() || cmake.empty()) {
        std::cerr << "Error: git and cmake are required to perform an update.\n";
        return false;
    }

    fs::path temp_dir = fs::temp_directory_path() / "vibe-fi-update";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    std::cout << "📥 Cloning latest repository source (" << latest_version << ")...\n";
    std::string clone_cmd = "git clone --depth 1 https://github.com/Swadesh-c0de/vibe-fi.git " + shell_escape(temp_dir.string());
    int ret = std::system(clone_cmd.c_str());
    if (ret != 0) {
        std::cerr << "Failed to clone repository.\n";
        fs::remove_all(temp_dir, ec);
        return false;
    }

    std::cout << "⚙️  Configuring Release build...\n";
    std::string build_dir = (temp_dir / "build").string();
    std::string cfg_cmd = "cmake -B " + shell_escape(build_dir) +
                          " -DCMAKE_BUILD_TYPE=Release -S " + shell_escape(temp_dir.string()) + " >/dev/null";
    ret = std::system(cfg_cmd.c_str());
    if (ret != 0) {
        std::cerr << "CMake build configuration failed.\n";
        fs::remove_all(temp_dir, ec);
        return false;
    }

    std::cout << "🔨 Compiling Vibe-Fi binary...\n";
    std::string build_cmd = "cmake --build " + shell_escape(build_dir) + " -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2) >/dev/null";
    ret = std::system(build_cmd.c_str());
    if (ret != 0) {
        std::cerr << "Compilation failed.\n";
        fs::remove_all(temp_dir, ec);
        return false;
    }

    fs::path built_bin = temp_dir / "build" / "vibe_fi";
    if (!fs::exists(built_bin)) {
        std::cerr << "Compiled binary not found.\n";
        fs::remove_all(temp_dir, ec);
        return false;
    }

    std::cout << "📦 Atomically installing updated binary...\n";
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
            std::cout << "🔒 Administrator permission required to write to " << target << "...\n";
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
            std::cout << "Could not fetch latest release info from GitHub (offline or rate-limited).\n";
        }
        return false;
    }

    std::string current = VIBE_FI_VERSION;
    if (!is_newer_version(latest, current)) {
        if (force_check) {
            std::cout << "✨ Vibe-Fi is already up to date (" << current << ")!\n";
        }
        return false;
    }

    // Display update notification banner
    std::cout << "\n\033[1;36m=============================================================\033[0m\n";
    std::cout << "  \033[1;33m✨ A new version of Vibe-Fi is available!\033[0m\n";
    std::cout << "     Current: \033[0;31m" << current << "\033[0m  ➔  Latest: \033[1;32m" << latest << "\033[0m\n";
    std::cout << "\033[1;36m=============================================================\033[0m\n";
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
            std::cout << "✅ Update completed successfully!\n";
            std::cout << "Resuming Vibe-Fi (" << latest << ")...\n\n";
            execvp(current_exe.c_str(), argv);
            execvp(argv[0], argv);
            return true;
        } else {
            std::cout << "⚠️  Update failed. Continuing with existing version...\n\n";
            return false;
        }
    } else {
        std::cout << "⏩ Skipping update for now. Starting Vibe-Fi...\n\n";
        return false;
    }
}
