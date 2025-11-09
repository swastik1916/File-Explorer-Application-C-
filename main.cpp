#include <iostream>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <sstream>

namespace fs = std::filesystem;

// ==========================
// Color codes (Linux style)
// ==========================
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

// ==========================
// Globals
// ==========================
std::string currentUser = "user";
bool sudoMode = false;
std::map<std::string, std::string> permissions;
const std::string PERM_FILE = ".permissions.txt";

// ==========================
// Load/Save Permissions
// ==========================
void loadPermissions() {
    std::ifstream fin(PERM_FILE);
    if (!fin) return;
    std::string file, perm;
    while (fin >> file >> perm)
        permissions[file] = perm;
    fin.close();
}

void savePermissions() {
    std::ofstream fout(PERM_FILE);
    for (auto &p : permissions)
        fout << p.first << " " << p.second << "\n";
    fout.close();
}

std::string getPermission(const std::string &file) {
    if (permissions.count(file)) return permissions[file];
    return "-rw-r--r--";
}

// ==========================
// Display Helpers
// ==========================
void printPermissions(const std::string &perm, const std::string &name) {
    std::string color = RESET;
    if (perm[0] == 'd') color = BLUE;
    else if (perm.find('w') != std::string::npos) color = GREEN;
    else color = YELLOW;

    std::cout << color << perm << "  " << name << RESET << "\n";
}

// ==========================
// Commands
// ==========================
void listFiles() {
    std::cout << "\n";
    for (auto &entry : fs::directory_iterator(fs::current_path())) {
        std::string name = entry.path().filename().string();
        printPermissions(getPermission(name), name);
    }
}

void makeDir(const std::string &name) {
    if (fs::create_directory(name)) {
        permissions[name] = "drwxr-xr-x";
        savePermissions();
        std::cout << GREEN << "Directory created: " << name << RESET << "\n";
    } else std::cout << RED << "Failed to create directory.\n" << RESET;
}

void removeDir(const std::string &name) {
    if (!fs::exists(name)) { std::cout << RED << "Not found.\n" << RESET; return; }
    if (!fs::is_directory(name)) { std::cout << RED << "Not a directory.\n" << RESET; return; }
    if (fs::is_empty(name)) {
        fs::remove(name);
        permissions.erase(name);
        savePermissions();
        std::cout << GREEN << "Directory removed.\n" << RESET;
    } else std::cout << YELLOW << "Directory not empty.\n" << RESET;
}

void deleteFile(const std::string &name) {
    if (!fs::exists(name)) { std::cout << RED << "File not found.\n" << RESET; return; }
    if (!sudoMode && getPermission(name)[2] != 'w') {
        std::cout << RED << "Permission denied.\n" << RESET; return;
    }
    fs::remove(name);
    permissions.erase(name);
    savePermissions();
    std::cout << GREEN << "Deleted: " << name << RESET << "\n";
}

void chmodFile(const std::string &name, const std::string &permCode) {
    if (!fs::exists(name)) { std::cout << RED << "Not found.\n" << RESET; return; }
    if (permCode.size() != 3) { std::cout << YELLOW << "Use format like 755.\n" << RESET; return; }

    auto conv = [](char c, bool dir=false) {
        int n = c - '0';
        std::string s;
        s += (dir ? 'd' : '-');
        s += (n & 4 ? 'r' : '-');
        s += (n & 2 ? 'w' : '-');
        s += (n & 1 ? 'x' : '-');
        return s;
    };

    bool isDir = fs::is_directory(name);
    std::string p = conv(permCode[0], isDir) + conv(permCode[1]) + conv(permCode[2]);
    permissions[name] = p;
    savePermissions();
    std::cout << GREEN << "Changed permission of " << name << " to " << p << RESET << "\n";
}

void showPerm(const std::string &name) {
    if (!fs::exists(name)) { std::cout << RED << "Not found.\n" << RESET; return; }
    std::cout << YELLOW << name << ": " << getPermission(name) << RESET << "\n";
}

void copyFileCmd(const std::string &src, const std::string &dest) {
    if (!fs::exists(src)) { std::cout << RED << "Source not found.\n" << RESET; return; }
    if (!sudoMode && getPermission(src)[1] != 'r') {
        std::cout << RED << "Permission denied (no read).\n" << RESET; return;
    }
    try {
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
        permissions[dest] = getPermission(src);
        savePermissions();
        std::cout << GREEN << "Copied " << src << " → " << dest << RESET << "\n";
    } catch (...) {
        std::cout << RED << "Copy failed.\n" << RESET;
    }
}

void moveFileCmd(const std::string &src, const std::string &dest) {
    if (!fs::exists(src)) { std::cout << RED << "Source not found.\n" << RESET; return; }
    if (!sudoMode && getPermission(src)[2] != 'w') {
        std::cout << RED << "Permission denied (no write).\n" << RESET; return;
    }
    try {
        fs::rename(src, dest);
        permissions[dest] = permissions[src];
        permissions.erase(src);
        savePermissions();
        std::cout << GREEN << "Moved " << src << " → " << dest << RESET << "\n";
    } catch (...) {
        std::cout << RED << "Move failed.\n" << RESET;
    }
}

// ==========================
// Help Command
// ==========================
void showHelp() {
    std::cout << "\n==============================\n";
    std::cout << " Available Commands (Linux-style)\n";
    std::cout << "==============================\n";
    std::cout << "ls                  - List directory contents with color + permissions\n";
    std::cout << "cd <dir>            - Change directory\n";
    std::cout << "mkdir <name>        - Create directory\n";
    std::cout << "rmdir <name>        - Remove directory (if empty)\n";
    std::cout << "del <file>          - Delete a file\n";
    std::cout << "chmod <file> <perm> - Change file permissions (rwx format)\n";
    std::cout << "perm <file>         - Show permissions\n";
    std::cout << "cp <src> <dest>     - Copy file to destination\n";
    std::cout << "mv <src> <dest>     - Move (rename) file or directory\n";
    std::cout << "sudo <cmd>          - Temporary permission override\n";
    std::cout << "help                - Show this help menu\n";
    std::cout << "exit                - Quit program\n";
    std::cout << "==============================\n";
}

// ==========================
// Command Loop
// ==========================
int main() {
    loadPermissions();

    std::cout << "==============================\n";
    std::cout << " FILE EXPLORER Application (Capstone Project)\n";
    std::cout << "==============================\n";
    std::cout << "Current Directory: " << fs::current_path().string() << "\n\n";

    std::string input;
    while (true) {
        std::cout << currentUser << "@explorer " << fs::current_path().filename().string() << " $ ";
        std::getline(std::cin, input);
        std::istringstream iss(input);
        std::string cmd, arg1, arg2;
        iss >> cmd >> arg1 >> arg2;

        if (cmd == "exit") break;
        else if (cmd == "ls") listFiles();
        else if (cmd == "mkdir") makeDir(arg1);
        else if (cmd == "rmdir") removeDir(arg1);
        else if (cmd == "del") deleteFile(arg1);
        else if (cmd == "chmod") chmodFile(arg1, arg2);
        else if (cmd == "perm") showPerm(arg1);
        else if (cmd == "cp") copyFileCmd(arg1, arg2);
        else if (cmd == "mv") moveFileCmd(arg1, arg2);
        else if (cmd == "help") showHelp();
        else if (cmd == "sudo") {
            sudoMode = true;
            std::cout << YELLOW << "Sudo mode active (for one command)." << RESET << "\n";
        }
        else if (cmd.empty()) continue;
        else std::cout << RED << "Unknown command.\n" << RESET;

        sudoMode = false; // sudo lasts one command
    }

    std::cout << YELLOW << "Exiting. Goodbye!\n" << RESET;
    return 0;
}
