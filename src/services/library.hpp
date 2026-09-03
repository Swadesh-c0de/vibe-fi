#ifndef LIBRARY_HPP
#define LIBRARY_HPP

#include <string>
#include <vector>
#include <unordered_map>

struct LibraryItem {
    std::string name;
    std::string path;
    std::string duration;
    bool is_directory;
};

class Library {
public:
    Library();
    void set_root(const std::string& path);
    std::vector<LibraryItem> list_directory(const std::string& path);
    std::vector<LibraryItem> search(const std::string& query);
    std::string get_home_music_dir();
    bool is_audio_file(const std::string& filename);

private:
    std::string root_path;
    std::unordered_map<std::string, std::string> duration_cache;
    bool fuzzy_match(const std::string& pattern, const std::string& text);
};

#endif // LIBRARY_HPP
