#ifndef UPDATER_HPP
#define UPDATER_HPP

#include <string>
#include <functional>
#include <cstdint>

#ifndef VIBE_FI_VERSION
#define VIBE_FI_VERSION "1.1.1"
#endif

/**
 * Checks GitHub Releases API for the latest published tag name (e.g. "v1.2.0").
 * Returns an empty string if offline, timed out, or unparseable.
 */
std::string check_latest_version(int timeout_seconds = 2);

/**
 * Compares two semantic version strings (e.g. "v1.2.0" vs "1.1.1").
 * Returns true if latest is strictly newer than current.
 */
bool is_newer_version(const std::string& latest, const std::string& current);

/**
 * Resolves the absolute path to the currently running executable.
 */
std::string get_current_executable_path(const char* argv0);

/**
 * Downloads, compiles, and installs the update atomically using `install -m 755`.
 * Strictly respects Invariant 5 (atomic binary replacement).
 */
bool perform_update(const std::string& latest_version, const std::string& current_exe_path);

/**
 * Checks for updates synchronously (for CLI flags like --update / -u).
 * If connected to an interactive TTY, prompts the user [y/N].
 * If user answers yes, updates the binary and execvp-replaces the current process.
 */
bool prompt_and_handle_update(int argc, char* argv[], bool force_check = false);

/**
 * Fast, 0-network latency startup prompt:
 * Reads cached available_update from ~/.vibe-fi/state.ini.
 * If a newer version was detected during a previous session and not dismissed, prompts [y/N].
 * Returns true if update was applied (and process was restarted), false otherwise.
 */
bool check_and_prompt_cached_update(int argc, char* argv[]);

/**
 * Spawns a background thread to check GitHub for updates while the user is listening to music.
 * Rate-limited to check at most once every 24 hours.
 * If a newer version is discovered, records it in ~/.vibe-fi/state.ini and notifies on_update_found.
 */
void start_background_update_check(std::function<void(const std::string&)> on_update_found = nullptr);

/**
 * Stops or detaches the background update thread before application exit.
 */
void stop_background_update_check();

/**
 * Interactively prompts and uninstalls Vibe-Fi binaries and optionally ~/.vibe-fi config.
 */
bool handle_uninstall(const char* argv0);

#endif // UPDATER_HPP
