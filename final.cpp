#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>

using namespace std;

// Represents a file's info in the system
struct FileEntry {
    char fileName[100];  // name of the file
    int startAddress;    // where the file data starts in memory
    int fileSize;        // how big the file is

    FileEntry() {
        startAddress = 0;
        fileSize = 0;
        // Clear the name just in case
        for (int i = 0; i < 100; i++) {
            fileName[i] = '\0';
        }
    }

    FileEntry(const string& name, int address, int size) {
        startAddress = address;
        fileSize = size;

        // Copy the name into fileName with null terminator
        int i;
        for (i = 0; i < 99 && i < name.length(); i++) {
            fileName[i] = name[i];
        }
        fileName[i] = '\0';
    }
};

// The main file system handler
class FileSystem {
private:
    static const int TOTAL_SIZE = 10 * 1024 * 1024;  // 10MB total space
    static const int DIR_SIZE = 1 * 1024 * 1024;     // 1MB just for directory stuff
    static const int DATA_SIZE = 9 * 1024 * 1024;    // 9MB for actual file content
    static const int MAX_FILES = 100;                // Max number of files allowed

    char* storage;                   // Full storage buffer
    string diskFileName;            // Filename used to store our "virtual disk"
    FileEntry directory[MAX_FILES]; // List of file entries
    int fileCount;                  // How many files we have
    int nextFreeAddress;            // Where to put the next file's data

public:
    FileSystem(const string& filename) {
        diskFileName = filename;
        fileCount = 0;

        storage = new char[TOTAL_SIZE];

        // Wipe storage clean
        for (int i = 0; i < TOTAL_SIZE; i++) {
            storage[i] = 0;
        }

        // Data starts after directory section
        nextFreeAddress = DIR_SIZE;

        // Try loading old data if it exists
        loadFromDisk();
    }

    ~FileSystem() {
        saveToDisk();
        delete[] storage;
    }

    // Make a new file with some data
    void createNewFile(const string& filename, const string& data) {
        if (findFile(filename) != nullptr) {
            cout << "\n!!! ERROR: File '" << filename << "' already exists !!! \n";
            return;
        }

        if (fileCount >= MAX_FILES) {
            cout << "\n*** SYSTEM LIMIT REACHED: Cannot store more than " << MAX_FILES << " files! ***\n";
            return;
        }

        int dataSize = data.length() + 1; // Include null terminator
        if (nextFreeAddress + dataSize > TOTAL_SIZE) {
            cout << "\n!!! WARNING: STORAGE FULL !!! Not enough room for this file!\n";
            return;
        }

        // Copy data into storage
        for (int i = 0; i < data.length(); i++) {
            storage[nextFreeAddress + i] = data[i];
        }
        storage[nextFreeAddress + data.length()] = '\0';

        // Add to directory
        FileEntry newFile(filename, nextFreeAddress, dataSize);
        directory[fileCount++] = newFile;

        nextFreeAddress += dataSize;

        cout << "\n>>> SUCCESS: File '" << filename << "' created successfully! <<<\n";

        saveToDisk();
    }

    // Show all saved files
    void listFiles() {
        cout << "\n=== FILES IN THE SYSTEM ===\n";
        cout << "===================================\n";

        if (fileCount == 0) {
            cout << "** No files found. Storage is empty! **\n";
            return;
        }

        cout << left << setw(4) << "#" << setw(40) << "FILENAME" << "SIZE\n";
        cout << "-----------------------------------\n";

        for (int i = 0; i < fileCount; i++) {
            cout << left << setw(4) << (i + 1) << setw(40) << directory[i].fileName
                << directory[i].fileSize << " bytes\n";
        }
        cout << "===================================\n";
        cout << "Total files: " << fileCount << "/" << MAX_FILES << "\n";
    }

    // View what's inside a file
    void viewFile(const string& filename) {
        FileEntry* file = findFile(filename);
        if (file == nullptr) {
            cout << "\n!!! ERROR: File '" << filename << "' not found! !!!\n";
            return;
        }

        cout << "\n=== CONTENTS OF '" << filename << "' ===\n";
        cout << "===================================\n";
        for (int i = 0; i < file->fileSize - 1; i++) {
            cout << storage[file->startAddress + i];
        }
        cout << "\n===================================\n";
    }

    // Delete a file from the system
    void deleteFile(const string& filename) {
        int fileIndex = -1;
        for (int i = 0; i < fileCount; i++) {
            if (strcmp(directory[i].fileName, filename.c_str()) == 0) {
                fileIndex = i;
                break;
            }
        }

        if (fileIndex == -1) {
            cout << "\n!!! ERROR: File '" << filename << "' not found! !!!\n";
            return;
        }

        // Just remove from directory, don't reclaim data space
        for (int i = fileIndex; i < fileCount - 1; i++) {
            directory[i] = directory[i + 1];
        }
        fileCount--;

        cout << "\n>>> File '" << filename << "' has been DELETED! <<<\n";
        saveToDisk();
    }

    // Main menu loop
    void runFileSystem() {
        int choice;
        bool running = true;

        while (running) {
            cout << "\n+===================================+\n";
            cout << "|   SUPER FILE STORAGE SYSTEM 3000   |\n";
            cout << "+===================================+\n";
            cout << "+-----------------------------------+\n";
            cout << "| 1. Create a new file              |\n";
            cout << "| 2. List files                     |\n";
            cout << "| 3. View file contents             |\n";
            cout << "| 4. Delete file                    |\n";
            cout << "| 5. Exit                           |\n";
            cout << "+-----------------------------------+\n";
            cout << "Enter your choice: ";

            cin >> choice;

            if (cin.fail()) {
                cin.clear();
                char c;
                while ((c = cin.get()) != '\n' && c != EOF) {}
                cout << "\n!!! CONFUSED !!! That's not a number I recognize! Try again.\n";
                continue;
            }

            char c;
            while ((c = cin.get()) != '\n' && c != EOF) {}

            string filename, data, line;

            switch (choice) {
            case 1:
                cout << ">> Enter filename: ";
                getline(cin, filename);

                cout << ">> Enter file content (type '###END###' to finish):\n";
                data = "";
                while (getline(cin, line)) {
                    if (line == "###END###") break;
                    data += line + "\n";
                }

                createNewFile(filename, data);
                break;

            case 2:
                listFiles();
                break;

            case 3:
                listFiles();
                cout << ">> Enter filename to view: ";
                getline(cin, filename);
                viewFile(filename);
                break;

            case 4:
                listFiles();
                cout << ">> Enter filename to delete: ";
                getline(cin, filename);
                deleteFile(filename);
                break;

            case 5:
                cout << "\n*** Thanks for using the SUPER FILE STORAGE SYSTEM 3000! Goodbye! ***\n";
                running = false;
                break;

            default:
                cout << "\n!!! INVALID CHOICE !!! Please select from the menu options (1-5)\n";
            }
        }
    }

private:
    // Helper to find file by name
    FileEntry* findFile(const string& filename) {
        for (int i = 0; i < fileCount; i++) {
            if (strcmp(directory[i].fileName, filename.c_str()) == 0) {
                return &directory[i];
            }
        }
        return nullptr;
    }

    // Load data from the disk file
    void loadFromDisk() {
        ifstream file(diskFileName.c_str(), ios::binary);
        if (!file) {
            cout << "*** No previous data found. Starting fresh! ***\n";
            return;
        }

        file.read(storage, TOTAL_SIZE);

        fileCount = *((int*)storage);
        nextFreeAddress = *((int*)(storage + 4));

        for (int i = 0; i < fileCount; i++) {
            FileEntry* entry = (FileEntry*)(storage + 8 + i * sizeof(FileEntry));
            directory[i] = *entry;
        }

        cout << ">>> Loaded file system with " << fileCount << " files successfully! <<<\n";
    }

    // Save everything to the disk file
    void saveToDisk() {
        *((int*)storage) = fileCount;
        *((int*)(storage + 4)) = nextFreeAddress;

        for (int i = 0; i < fileCount; i++) {
            FileEntry* entry = (FileEntry*)(storage + 8 + i * sizeof(FileEntry));
            *entry = directory[i];
        }

        ofstream file(diskFileName.c_str(), ios::binary);
        if (!file) {
            cerr << "\n!!! CRITICAL ERROR !!! Couldn't save to " << diskFileName << "!\n";
            return;
        }

        file.write(storage, TOTAL_SIZE);
        file.close();
    }
};

int main() {
    FileSystem fs("simpledisk.bin");
    fs.runFileSystem();
    return 0;
}
