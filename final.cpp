#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>
#include <vector>       // needed for dynamic arrays
#include <list>         // needed for linked lists
#include <queue>        // needed for queue operations
#include <stack>        // needed for stack operations
#include <algorithm>    // needed for sorting and other algorithms

using namespace std;

// system specs - we're making a 10MB file system!
const int TOTAL_SIZE = 10 * 1024 * 1024;   // 10MB total size (wow that's a lot)
const int DIR_SECTION_SIZE = 1 * 1024 * 1024;   // 1MB for directory entries
const int FREE_LIST_SIZE = 1 * 1024 * 1024;     // 1MB for free block tracking
const int DATA_SECTION_SIZE = 8 * 1024 * 1024;  // 8MB for actual file data

const int BLOCK_SIZE = 1024;                    // each block is 1KB (pretty standard)
const int MAX_BLOCKS = DATA_SECTION_SIZE / BLOCK_SIZE;  // total number of blocks available
const int DIR_ENTRY_SIZE = 500;                 // each file entry takes 500 bytes in directory
const int MAX_FILES = DIR_SECTION_SIZE / DIR_ENTRY_SIZE; // max number of files we can store
const int FILE_NAME_MAX = 100;                  // max length of filenames (should be enough)

// this is like the "inode" structure that stores file metadata
struct DirectoryEntry {
    char fileName[FILE_NAME_MAX]; // File name
    int startBlock;               // Starting block index of the file data
    int fileSize;                 // Size of the file in bytes
    bool isValid;                 // Flag to check if this entry is valid

    DirectoryEntry() {
        startBlock = -1;          // initialize with invalid block
        fileSize = 0;             // empty file to start
        isValid = false;          // not a valid entry yet

        // clear the filename with null chars (good practice to avoid garbage)
        for (int i = 0; i < FILE_NAME_MAX; i++) {
            fileName[i] = '\0';
        }
    }
};

// structure to represent a block of file data (like a chunk of a file)
struct FileBlock {
    int blockIndex;       // Index of this block
    int nextBlockIndex;   // Index of the next block, -1 if last block
    vector<char> data;    // Block data using vector instead of raw array

    FileBlock(int index) : blockIndex(index), nextBlockIndex(-1) {
        // leave room for the "next block" pointer at beginning of block
        data.resize(BLOCK_SIZE - 4, 0); // make space for data minus next pointer
    }
};

// main class that handles the entire file system
class FileSystem {
private:
    char* storage;                          // Full 10MB storage buffer
    string diskFileName;                    // Name of the file that represents our disk
    vector<bool> freeBlocks;                // vector that tracks which blocks are free (true = free)
    int freeBlockCount;                     // Number of free blocks

    list<int> freeBlockList;                // list of free block indices

public:
    FileSystem(const string& filename) {
        diskFileName = filename;
        storage = new char[TOTAL_SIZE];     // allocate the virtual disk in memory

        
        for (int i = 0; i < TOTAL_SIZE; i++)    // initialize everything to zeros (clean slate)
            storage[i] = 0;
        
        freeBlocks.resize(MAX_BLOCKS, true);    // start with all blocks free
        freeBlockCount = MAX_BLOCKS;

        
        for (int i = 0; i < MAX_BLOCKS; i++)    // populate the free block list (all blocks start free)
            freeBlockList.push_back(i);

        
        if (!loadFromDisk()) {               // try to load existing file system or create new one
            cout << "*** Starting with a fresh file system ***\n";
            initializeFileSystem();
        }
    }

    ~FileSystem() {
        saveToDisk();            // make sure everything is saved
        delete[] storage;        // free memory to avoid leaks
    }

    // set up a brand new file system
    void initializeFileSystem() {
        for (int i = 0; i < DIR_SECTION_SIZE; i++)  // clear out the directory section
            storage[i] = 0;

        updateFreeBlockList();

        
        for (int i = 0; i < DATA_SECTION_SIZE; i++)     // clear data section too (might be unnecessary but better safe)
            storage[DIR_SECTION_SIZE + FREE_LIST_SIZE + i] = 0;
    }

    int getDataSectionStart() const {
        return DIR_SECTION_SIZE + FREE_LIST_SIZE;    // directory + free list, that's where data starts
    }

    // convert from block number to actual memory address
    int getBlockAddress(int blockIndex) const {
        return getDataSectionStart() + (blockIndex * BLOCK_SIZE);    // offset from start of data section
    }

    // save the free block info into the storage
    void updateFreeBlockList() {
        // store the count of free blocks at beginning of that section
        int* freeBlockCountPtr = (int*)(storage + DIR_SECTION_SIZE);
        *freeBlockCountPtr = freeBlockCount;

        for (int i = 0; i < MAX_BLOCKS; i++)   // store which blocks are free and which are used
            storage[DIR_SECTION_SIZE + 4 + i] = freeBlocks[i] ? 1 : 0;    // 1 = free, 0 = used
    }

    // get a free block for storing data (like malloc but for blocks)
    int allocateBlock() {
        if (freeBlockList.empty()) {
            return -1;    // THIS MEANS THAT DISK IS FULL
        }

        // get first available block from the list (front is faster than searching)
        int blockIndex = freeBlockList.front();
        freeBlockList.pop_front();

        // mark it as used now
        freeBlocks[blockIndex] = false;
        freeBlockCount--;
        updateFreeBlockList();

        return blockIndex;
    }

    // return a block to the free pool (like free() but for blocks)
    void freeBlock(int blockIndex) {
        if (blockIndex >= 0 && blockIndex < MAX_BLOCKS && !freeBlocks[blockIndex]) {
            freeBlocks[blockIndex] = true;    // mark as free
            freeBlockCount++;
         
            freeBlockList.push_back(blockIndex);    // add to free list for reuse

            // cleaning out all the data to be safe
            int blockAddr = getBlockAddress(blockIndex);
            for (int i = 0; i < BLOCK_SIZE; i++) {
                storage[blockAddr + i] = 0;
            }

            updateFreeBlockList();
        }
    }

    // find an unused directory entry for a new file
    int findFreeDirectoryEntry() {
        DirectoryEntry* dirSection = (DirectoryEntry*)storage;

        for (int i = 0; i < MAX_FILES; i++) {
            if (!dirSection[i].isValid) {
                return i;    // found an empty slot
            }
        }
        return -1;    // directory is full, can't add more files
    }

    // look up a file by name
    DirectoryEntry* findFile(const string& filename) {
        DirectoryEntry* dirSection = (DirectoryEntry*)storage;

        for (int i = 0; i < MAX_FILES; i++) {
            if (dirSection[i].isValid && strcmp(dirSection[i].fileName, filename.c_str()) == 0) {
                return &dirSection[i];    // found it!
            }
        }
        return nullptr;    // file doesn't exist
    }

    // retrieve all the blocks that make up a file (follows the linked list)
    list<FileBlock> getFileBlocks(int startBlock) {
        list<FileBlock> blocks;
        int currentBlock = startBlock;

        // follow the chain of blocks until we hit the end (-1)
        while (currentBlock != -1) {
            FileBlock block(currentBlock);
            int blockAddr = getBlockAddress(currentBlock);

            // first 4 bytes have the next block pointer
            int* nextBlockPtr = (int*)(storage + blockAddr);
            block.nextBlockIndex = *nextBlockPtr;

            // copy the actual data into our block structure
            for (int i = 0; i < BLOCK_SIZE - 4; i++) {
                block.data[i] = storage[blockAddr + 4 + i];
            }

            blocks.push_back(block);
            currentBlock = block.nextBlockIndex;    // move to next block in chain
        }

        return blocks;
    }

    // create a new file with content (the main way to add files)
    bool createNewFile(const string& filename, const string& content) {
        // don't allow duplicate filenames
        if (findFile(filename) != nullptr) {
            cout << "\n!!! ERROR: File '" << filename << "' already exists !!!\n";
            return false;
        }

        // make sure we have room in the directory
        int dirIndex = findFreeDirectoryEntry();
        if (dirIndex == -1) {
            cout << "\n!!! ERROR: Directory is full, cannot create more files !!!\n";
            return false;
        }

        // figure out how many blocks we'll need for this content
        int contentSize = content.length() + 1;    // +1 for null terminator
        int blocksNeeded = (contentSize + BLOCK_SIZE - 5) / (BLOCK_SIZE - 4);    // ceiling division with space for next pointer

        // check if we have enough space
        if (blocksNeeded > freeBlockCount) {
            cout << "\n!!! ERROR: Not enough free space for file '" << filename << "' !!!\n";
            return false;
        }

        // use a queue to store all the blocks we'll use
        queue<int> blocksToWrite;
        int startBlock = -1;

        // first pass: allocate all the blocks we need
        for (int i = 0; i < blocksNeeded; i++) {
            int newBlock = allocateBlock();
            if (newBlock == -1) {
                // something went wrong, free any blocks we got already
                while (!blocksToWrite.empty()) {
                    freeBlock(blocksToWrite.front());
                    blocksToWrite.pop();
                }
                cout << "\n!!! ERROR: Failed to allocate blocks for file '" << filename << "' !!!\n";
                return false;
            }

            if (startBlock == -1) {
                startBlock = newBlock;    // remember the first block
            }

            blocksToWrite.push(newBlock);
        }

        // second pass: actually write the data to blocks
        int bytesWritten = 0;
        int remainingBytes = contentSize;

        // link all the blocks together as we go
        while (!blocksToWrite.empty()) {
            int currentBlock = blocksToWrite.front();
            blocksToWrite.pop();

            int blockAddr = getBlockAddress(currentBlock);
            int bytesToWrite = min(BLOCK_SIZE - 4, remainingBytes);

            // set the next block pointer (or -1 if last block)
            int* nextBlockPtr = (int*)(storage + blockAddr);
            *nextBlockPtr = blocksToWrite.empty() ? -1 : blocksToWrite.front();

            // copy this chunk of content to the block
            for (int i = 0; i < bytesToWrite; i++) {
                if (bytesWritten + i < content.length()) {
                    storage[blockAddr + 4 + i] = content[bytesWritten + i];
                }
                else {
                    storage[blockAddr + 4 + i] = '\0';    // null terminator at end
                }
            }

            bytesWritten += bytesToWrite;
            remainingBytes -= bytesToWrite;
        }

        // update the directory entry with the new file info
        DirectoryEntry* dirSection = (DirectoryEntry*)storage;
        strcpy_s(dirSection[dirIndex].fileName, filename.c_str());
        dirSection[dirIndex].startBlock = startBlock;
        dirSection[dirIndex].fileSize = contentSize;
        dirSection[dirIndex].isValid = true;

        cout << "\n>>> SUCCESS: File '" << filename << "' created successfully! <<<\n";
        return true;
    }

    // show list of all files in the system
    void listFiles() {
        DirectoryEntry* dirSection = (DirectoryEntry*)storage;

        // we'll use a vector to hold valid files for sorting
        vector<DirectoryEntry*> validFiles;

        // find all valid files first
        for (int i = 0; i < MAX_FILES; i++) {
            if (dirSection[i].isValid) {
                validFiles.push_back(&dirSection[i]);
            }
        }

        cout << "\n=== FILES IN THE SYSTEM ===\n";
        cout << "===================================\n";

        if (validFiles.empty()) {
            cout << "** No files found. Storage is empty! **\n";
        }
        else {
            // sort files by name (alphabetically)
            sort(validFiles.begin(), validFiles.end(),
                [](const DirectoryEntry* a, const DirectoryEntry* b) {
                    return strcmp(a->fileName, b->fileName) < 0;
                });

            // display each file info
            for (size_t i = 0; i < validFiles.size(); i++) {
                cout << left << setw(4) << (i + 1) << setw(40) << validFiles[i]->fileName
                    << validFiles[i]->fileSize << " bytes\n";
            }
        }

        cout << "===================================\n";
        cout << "Total files: " << validFiles.size() << "/" << MAX_FILES << "\n";
        cout << "Free blocks: " << freeBlockCount << "/" << MAX_BLOCKS << "\n";
    }

    // read a file's content as a string
    string readFile(const string& filename) {
        DirectoryEntry* file = findFile(filename);
        if (file == nullptr) {
            cout << "\n!!! ERROR: File '" << filename << "' not found! !!!\n";
            return "";
        }

        string content = "";

        // get all the blocks for this file
        list<FileBlock> blocks = getFileBlocks(file->startBlock);

        // reconstruct the file by appending each block's data
        for (list<FileBlock>::const_iterator blockIt = blocks.begin(); blockIt != blocks.end(); ++blockIt) {
            const FileBlock& block = *blockIt;
            for (size_t i = 0; i < block.data.size(); ++i) {
                char ch = block.data[i];
                if (ch == '\0') {
                    break;    // hit the end of the data
                }
                content += ch;
            }
        }

        return content;
    }

    // display a file's content
    void viewFile(const string& filename) {
        string content = readFile(filename);

        if (!content.empty()) {
            cout << "\n=== CONTENTS OF '" << filename << "' ===\n";
            cout << "===================================\n";
            cout << content << "\n";
            cout << "===================================\n";
        }
    }

    // delete a file and free its blocks
    bool deleteFile(const string& filename) {
        DirectoryEntry* dirSection = (DirectoryEntry*)storage;
        int fileIndex = -1;

        // find which directory entry has this file
        for (int i = 0; i < MAX_FILES; i++) {
            if (dirSection[i].isValid && strcmp(dirSection[i].fileName, filename.c_str()) == 0) {
                fileIndex = i;
                break;
            }
        }

        if (fileIndex == -1) {
            cout << "\n!!! ERROR: File '" << filename << "' not found! !!!\n";
            return false;
        }

        // use a stack so we free blocks in reverse order (just for fun honestly)
        stack<int> blocksToFree;
        int currentBlock = dirSection[fileIndex].startBlock;

        // follow the chain to find all blocks
        while (currentBlock != -1) {
            blocksToFree.push(currentBlock);
            int blockAddr = getBlockAddress(currentBlock);
            int nextBlock = *(int*)(storage + blockAddr);
            currentBlock = nextBlock;
        }

        // free each block
        while (!blocksToFree.empty()) {
            freeBlock(blocksToFree.top());
            blocksToFree.pop();
        }

        // mark directory entry as deleted
        dirSection[fileIndex].isValid = false;

        cout << "\n>>> File '" << filename << "' has been DELETED! <<<\n";
        return true;
    }

    // copy file from real OS into our virtual file system
    bool copyFromWindows(const string& windowsFileName) {
        ifstream inFile(windowsFileName.c_str());
        if (!inFile) {
            cout << "\n!!! ERROR: Cannot open file '" << windowsFileName << "' from Windows! !!!\n";
            return false;
        }

        // read the file line by line
        string line;
        string content = "";
        while (getline(inFile, line)) {
            content = content + line + "\n";    // keep the newlines
        }
        inFile.close();

        // extract just the filename part
        string fileName = "";
        int i;
        for (i = windowsFileName.length() - 1; i >= 0; i--) {
            char c = windowsFileName[i];
            if (c == '/' || c == '\\') {
                break;    // found directory separator
            }
        }
        // copy everything after the last slash
        for (int j = i + 1; j < windowsFileName.length(); j++)
            fileName = fileName + windowsFileName[j];

        return createNewFile(fileName, content);
    }

    // copy file from our virtual file system to real OS
    bool copyToWindows(const string& filename) {
        string content = readFile(filename); // read the file content first
        if (content.empty()) {
            return false;    // couldn't read file
        }

        // try to create the Windows file
        ofstream outFile(filename.c_str());
        if (!outFile) {
            cout << "\n!!! ERROR: Cannot create file '" << filename << "' in Windows! !!!\n";
            return false;
        }

        outFile << content;    // write the content
        outFile.close();

        cout << "\n>>> File '" << filename << "' has been copied to Windows! <<<\n";
        return true;
    }

    // modify a file by adding content to the end
    bool modifyFile(const string& filename, const string& appendContent) {
        string content = readFile(filename);        // get current content
        if (content.empty()) {
            return false;    // file not found or empty
        }

        deleteFile(filename);    // delete the old file

        // create new file with combined content
        return createNewFile(filename, content + appendContent);
    }

    // save the virtual filesystem to a real file
    void saveToDisk() {
        ofstream outFile(diskFileName.c_str(), ios::binary);
        if (!outFile) {
            cerr << "\n!!! CRITICAL ERROR !!! Couldn't save to " << diskFileName << "!\n";
            return;
        }

        outFile.write(storage, TOTAL_SIZE);    // dump the whole thing to disk
        outFile.close();

        cout << "\n>>> File system saved to disk <<<\n";
    }

    // load a virtual filesystem from a real file
    bool loadFromDisk() {
        ifstream inFile(diskFileName.c_str(), ios::binary);
        if (!inFile) {
            return false;    // file doesn't exist yet
        }

        inFile.read(storage, TOTAL_SIZE);    // load everything into memory
        inFile.close();

        // get the free block count from storage
        int* storedFreeBlockCount = (int*)(storage + DIR_SECTION_SIZE);
        freeBlockCount = *storedFreeBlockCount;

        // rebuild the free block tracking data
        freeBlockList.clear();
        freeBlocks.clear();
        freeBlocks.resize(MAX_BLOCKS, false);

        // recreate the free block list and status
        for (int i = 0; i < MAX_BLOCKS; i++) {
            bool isFree = (storage[DIR_SECTION_SIZE + 4 + i] == 1);
            freeBlocks[i] = isFree;

            if (isFree) {
                freeBlockList.push_back(i);    // add to free list if block is available
            }
        }

        cout << "\n>>> File system loaded from disk <<<\n";
        return true;
    }

    // THE INTERFACE
    void runFileSystem() {
        int choice;
        bool running = true;

        while (running) {
            cout << "\n+===================================+\n";
            cout << "|   SUPER FILE STORAGE SYSTEM 3000   |\n";
            cout << "+===================================+\n";
            cout << "+-----------------------------------+\n";
            cout << "| 1. Create a new file              |\n";
            cout << "| 2. List & view existing files     |\n";
            cout << "| 3. Copy file from Windows (*.txt) |\n";
            cout << "| 4. Copy file to Windows (*.txt)   |\n";
            cout << "| 5. Modify file                    |\n";
            cout << "| 6. Delete file                    |\n";
            cout << "| 7. Exit                           |\n";
            cout << "+-----------------------------------+\n";
            cout << "Enter your choice: ";

            cin >> choice;

            if (cin.fail()) {
                cin.clear();    // clear error flag
                cin.ignore(10000, '\n');    // clear input buffer
                cout << "\n!!! CONFUSED !!! That's not a number I recognize! Try again.\n";
                continue;
            }

            cin.ignore(10000, '\n');    // clear leftover newline

            string filename, data, line, path;

            switch (choice) {
            case 1: // Create a new file
                cout << ">> Enter filename: ";
                getline(cin, filename);

                cout << ">> Enter file content (type '###END###' on a new line to finish):\n";
                data = "";
                while (getline(cin, line)) {
                    if (line == "###END###") break;    // we use this marker to end input
                    data += line + "\n";
                }

                createNewFile(filename, data);    // make the file with entered content
                break;

            case 2: // List & view existing files
                listFiles();    // show what's in the system

                cout << "\n>> Enter filename to view (or press Enter to return to menu): ";
                getline(cin, filename);

                if (!filename.empty()) {
                    viewFile(filename);    // show the contents of selected file
                }
                break;

            case 3: // Copy from Windows
                cout << ">> Enter path and filename to copy from Windows: ";
                getline(cin, path);

                copyFromWindows(path);    // import from real OS
                break;

            case 4: // Copy to Windows
                listFiles();    // show available files

                cout << ">> Enter filename to copy to Windows: ";
                getline(cin, filename);

                if (!filename.empty()) {
                    copyToWindows(filename);    // export to real OS
                }
                break;

            case 5: // Modify file
                listFiles();    // show available files

                cout << ">> Enter filename to modify: ";
                getline(cin, filename);

                if (!filename.empty()) {
                    cout << ">> Enter additional content to append (type '###END###' on a new line to finish):\n";
                    data = "";
                    while (getline(cin, line)) {
                        if (line == "###END###") break;
                        data += line + "\n";
                    }

                    modifyFile(filename, data);    // add the new content to file
                }
                break;

            case 6: // Delete file
                listFiles();    // show available files

                cout << ">> Enter filename to delete: ";
                getline(cin, filename);

                if (!filename.empty()) {
                    deleteFile(filename);    // remove the file
                }
                break;

            case 7: // Exit
                cout << "\n*** Thanks for using the SUPER FILE STORAGE SYSTEM 3000! Goodbye! ***\n";
                running = false;    // exit the loop
                break;

            default:
                cout << "\n!!! INVALID CHOICE !!! Please select from the menu options (1-7)\n";
            }
        }
    }
};

int main() {
    FileSystem fs("simpledisk.bin");
    fs.runFileSystem();
    return 0;
}
