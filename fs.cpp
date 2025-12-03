#include <iostream>
#include "fs.h"
#include <cstring>
#include <string>


FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{

}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    std::cout << "FS::format()\n";

    for (int i = 0; i < BLOCK_SIZE / 2; i++) {
        fat[i] = FAT_FREE;
    }
    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK]  = FAT_EOF;

    unsigned char fat_block[BLOCK_SIZE];
    std::memcpy(fat_block, fat, BLOCK_SIZE);
    disk.write(FAT_BLOCK, fat_block);

    unsigned char dir_block[BLOCK_SIZE];
    std::memset(dir_block, 0, BLOCK_SIZE);
    disk.write(ROOT_BLOCK, dir_block);

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
std::cout << "FS::create(" << filepath << ")\n";

    if (filepath.find('/') != std::string::npos) {
        return -1;
    }

    unsigned char fat_block[BLOCK_SIZE];
    if (disk.read(FAT_BLOCK, fat_block) == 0) {
        std::memcpy(fat, fat_block, BLOCK_SIZE);
    }

    unsigned char dir_block[BLOCK_SIZE];
    if (disk.read(ROOT_BLOCK, dir_block) != 0) {
        return -2;
    }
    dir_entry *entries = reinterpret_cast<dir_entry*>(dir_block);
    int n = BLOCK_SIZE / sizeof(dir_entry);

    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] != '\0' &&
            filepath == entries[i].file_name) {
            return -3;
        }
    }

    int free_index = -1;
    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] == '\0') {
            free_index = i;
            break;
        }
    }
    if (free_index == -1) {
        return -4;
    }

    std::cout << "Enter data. Empty line to end.\n";
    std::string data;
    std::string line;
    while (true) {
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            break;
        }
        data += line;
        data += '\n';
    }

    uint32_t size = static_cast<uint32_t>(data.size());
    uint16_t first_blk = 0;

    if (size > 0) {
        const char *p = data.c_str();
        uint32_t left = size;
        int prev = -1;

        while (left > 0) {
            int b = -1;
            for (int i = 2; i < BLOCK_SIZE / 2; i++) {
                if (fat[i] == FAT_FREE) {
                    b = i;
                    break;
                }
            }
            if (b == -1) {
                return -5;
            }

            if (first_blk == 0) {
                first_blk = static_cast<uint16_t>(b);
            }
            if (prev != -1) {
                fat[prev] = static_cast<int16_t>(b);
            }

            unsigned char data_block[BLOCK_SIZE];
            std::memset(data_block, 0, BLOCK_SIZE);
            uint32_t to_copy = (left > BLOCK_SIZE) ? BLOCK_SIZE : left;
            std::memcpy(data_block, p, to_copy);
            disk.write(b, data_block);

            left -= to_copy;
            p    += to_copy;
            prev  = b;
        }

        if (prev != -1) {
            fat[prev] = FAT_EOF;
        }

        std::memcpy(fat_block, fat, BLOCK_SIZE);
        disk.write(FAT_BLOCK, fat_block);
    }

    dir_entry &e = entries[free_index];
    std::memset(&e, 0, sizeof(dir_entry));
    std::strncpy(e.file_name, filepath.c_str(), sizeof(e.file_name) - 1);
    e.file_name[sizeof(e.file_name) - 1] = '\0';
    e.size      = size;
    e.first_blk = first_blk;
    e.type      = TYPE_FILE;
    e.access_rights = READ | WRITE;

    disk.write(ROOT_BLOCK, dir_block);

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    unsigned char fat_block[BLOCK_SIZE];
    if (disk.read(FAT_BLOCK, fat_block) == 0) {
        std::memcpy(fat, fat_block, BLOCK_SIZE);
    }

    unsigned char dir_block[BLOCK_SIZE];
    if (disk.read(ROOT_BLOCK, dir_block) != 0) {
        return -1;
    }
    dir_entry *entries = reinterpret_cast<dir_entry*>(dir_block);
    int n = BLOCK_SIZE / sizeof(dir_entry);

    int index = -1;
    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] != '\0' &&
            filepath == entries[i].file_name) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return -2;
    }

    dir_entry &e = entries[index];
    if (e.type != TYPE_FILE) {
        return -3;
    }
    if (e.size == 0) {
        return 0;
    }

    uint32_t left = e.size;
    int block = e.first_blk;
    unsigned char data_block[BLOCK_SIZE];

    while (block != FAT_EOF && left > 0) {
        if (disk.read(block, data_block) != 0) {
            return -4;
        }
        uint32_t to_print = (left > BLOCK_SIZE) ? BLOCK_SIZE : left;
        std::cout.write(reinterpret_cast<char*>(data_block), to_print);
        left  -= to_print;
        block = fat[block];
    }

    std::cout << std::endl;
    return 0;

}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    std::cout << "FS::ls()\n";

    unsigned char dir_block[BLOCK_SIZE];
    if (disk.read(ROOT_BLOCK, dir_block) != 0) {
        return -1;
    }

    dir_entry *entries = reinterpret_cast<dir_entry*>(dir_block);
    int n = BLOCK_SIZE / sizeof(dir_entry);

    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] != '\0') {
            std::cout << entries[i].file_name
                      << " "
                      << entries[i].size
                      << "\n";
        }
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";

    unsigned char fat_block[BLOCK_SIZE];
    if (disk.read(FAT_BLOCK, fat_block) == 0) {
        std::memcpy(fat, fat_block, BLOCK_SIZE);
    }

    unsigned char dir_block[BLOCK_SIZE];
    if (disk.read(ROOT_BLOCK, dir_block) != 0) {
        return -1;
    }

    dir_entry *entries = reinterpret_cast<dir_entry*>(dir_block);
    int n = BLOCK_SIZE / sizeof(dir_entry);

    int src_i = -1;
    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] != '\0' && sourcepath == entries[i].file_name) {
            src_i = i;
            break;
        }
    }
    if (src_i == -1) return -2;

    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] != '\0' && destpath == entries[i].file_name) {
            return -3;
        }
    }

    int free_i = -1;
    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] == '\0') {
            free_i = i;
            break;
        }
    }
    if (free_i == -1) return -4;

    dir_entry &src = entries[src_i];

    uint32_t left = src.size;
    int blk = src.first_blk;

    uint16_t new_first = 0;
    int prev = -1;

    while (blk != FAT_EOF && left > 0) {
        int nb = -1;
        for (int i = 2; i < BLOCK_SIZE / 2; i++) {
            if (fat[i] == FAT_FREE) {
                nb = i;
                break;
            }
        }
        if (nb == -1) return -5;

        unsigned char data_block[BLOCK_SIZE];
        disk.read(blk, data_block);
        disk.write(nb, data_block);

        if (new_first == 0) new_first = nb;
        if (prev != -1) fat[prev] = nb;

        prev = nb;
        blk = fat[blk];
        left -= BLOCK_SIZE;
    }

    fat[prev] = FAT_EOF;

    std::memcpy(fat_block, fat, BLOCK_SIZE);
    disk.write(FAT_BLOCK, fat_block);

    dir_entry &d = entries[free_i];
    std::memset(&d, 0, sizeof(dir_entry));
    std::strncpy(d.file_name, destpath.c_str(), sizeof(d.file_name)-1);
    d.size = src.size;
    d.first_blk = new_first;
    d.type = TYPE_FILE;
    d.access_rights = src.access_rights;

    disk.write(ROOT_BLOCK, dir_block);

    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    unsigned char dir_block[BLOCK_SIZE];
    if (disk.read(ROOT_BLOCK, dir_block) != 0) {
        return -1;
    }

    dir_entry *entries = reinterpret_cast<dir_entry*>(dir_block);
    int n = BLOCK_SIZE / sizeof(dir_entry);

    int src_i = -1;
    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] != '\0' && sourcepath == entries[i].file_name) {
            src_i = i;
            break;
        }
    }
    if (src_i == -1) return -2;

    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] != '\0' && destpath == entries[i].file_name) {
            return -3;
        }
    }

    dir_entry &e = entries[src_i];
    std::memset(e.file_name, 0, sizeof(e.file_name));
    std::strncpy(e.file_name, destpath.c_str(), sizeof(e.file_name)-1);

    disk.write(ROOT_BLOCK, dir_block);

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";

    unsigned char fat_block[BLOCK_SIZE];
    disk.read(FAT_BLOCK, fat_block);
    std::memcpy(fat, fat_block, BLOCK_SIZE);

    unsigned char dir_block[BLOCK_SIZE];
    if (disk.read(ROOT_BLOCK, dir_block) != 0) {
        return -1;
    }

    dir_entry *entries = reinterpret_cast<dir_entry*>(dir_block);
    int n = BLOCK_SIZE / sizeof(dir_entry);

    int i = -1;
    for (int k = 0; k < n; k++) {
        if (entries[k].file_name[0] != '\0' &&
            filepath == entries[k].file_name) {
            i = k;
            break;
        }
    }
    if (i == -1) return -2;

    dir_entry &e = entries[i];

    int blk = e.first_blk;
    while (blk != FAT_EOF && blk >= 0) {
        int next = fat[blk];
        fat[blk] = FAT_FREE;
        blk = next;
    }

    std::memset(&e, 0, sizeof(dir_entry));

    std::memcpy(fat_block, fat, BLOCK_SIZE);
    disk.write(FAT_BLOCK, fat_block);
    disk.write(ROOT_BLOCK, dir_block);

    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";

    unsigned char fat_block[BLOCK_SIZE];
    disk.read(FAT_BLOCK, fat_block);
    std::memcpy(fat, fat_block, BLOCK_SIZE);

    unsigned char dir_block[BLOCK_SIZE];
    if (disk.read(ROOT_BLOCK, dir_block) != 0) return -1;

    dir_entry *entries = reinterpret_cast<dir_entry*>(dir_block);
    int n = BLOCK_SIZE / sizeof(dir_entry);

    int i1 = -1, i2 = -1;
    for (int i = 0; i < n; i++) {
        if (entries[i].file_name[0] != '\0' && filepath1 == entries[i].file_name)
            i1 = i;
        if (entries[i].file_name[0] != '\0' && filepath2 == entries[i].file_name)
            i2 = i;
    }
    if (i1 == -1 || i2 == -1) return -2;

    dir_entry &A = entries[i1];
    dir_entry &B = entries[i2];

    int end = B.first_blk;
    if (end != 0) {
        while (fat[end] != FAT_EOF) {
            end = fat[end];
        }
    } else {
        int nb = -1;
        for (int j = 2; j < BLOCK_SIZE / 2; j++) {
            if (fat[j] == FAT_FREE) { nb = j; break; }
        }
        if (nb == -1) return -3;
        B.first_blk = nb;
        fat[nb] = FAT_EOF;
        end = nb;
    }

    uint32_t left = A.size;
    int blk = A.first_blk;
    unsigned char buf[BLOCK_SIZE];

    while (blk != FAT_EOF && left > 0) {
        disk.read(blk, buf);

        int nb = -1;
        for (int j = 2; j < BLOCK_SIZE / 2; j++) {
            if (fat[j] == FAT_FREE) { nb = j; break; }
        }
        if (nb == -1) return -4;

        disk.write(nb, buf);

        fat[end] = nb;
        end = nb;
        fat[end] = FAT_EOF;

        left -= BLOCK_SIZE;
        blk = fat[blk];
    }

    B.size += A.size;

    std::memcpy(fat_block, fat, BLOCK_SIZE);
    disk.write(FAT_BLOCK, fat_block);
    disk.write(ROOT_BLOCK, dir_block);

    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
