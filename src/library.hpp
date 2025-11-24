#ifndef LIBRARY_HPP
#define LIBRARY_HPP

#include <string>
#include <vector>
#include <filesystem>

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

private:
    std::string root_path;
};

#endif // LIBRARY_HPP
