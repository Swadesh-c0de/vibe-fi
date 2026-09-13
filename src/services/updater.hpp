#ifndef UPDATER_HPP
#define UPDATER_HPP

#include <string>

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
 * Checks for updates on startup if connected to an interactive TTY.
 * If a newer version is available, prompts the user [y/N].
 * If user answers yes, updates the binary and execvp-replaces the current process.
 * If user answers no or is already on latest, returns false so startup continues.
 */
bool prompt_and_handle_update(int argc, char* argv[], bool force_check = false);

#endif // UPDATER_HPP
