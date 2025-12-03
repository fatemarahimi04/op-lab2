#include <iostream>
#include "fs.h"
#include <cstring>
#include <algorithm>


FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{

}

// formats the disk, i.e., creates an empty file system
int FS::format()
{
    int fat_entries = BLOCK_SIZE / 2;

    for (int i = 0; i < fat_entries; i++)
        fat[i] = FAT_FREE;

    fat[ROOT_BLOCK] = FAT_EOF;
    fat[FAT_BLOCK] = FAT_EOF;

    save_fat();

    dir_entry entries[BLOCK_SIZE / sizeof(dir_entry)];
    std::memset(entries, 0, sizeof(entries));

    write_dir(ROOT_BLOCK, entries);
    current_dir = ROOT_BLOCK;

    return 0;
}


// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath)
{
    // 1. Läs katalogen
    dir_entry entries[BLOCK_SIZE / sizeof(dir_entry)];
    read_dir(current_dir, entries);

    // 2. Kolla om filen redan finns
    if (find_dir_entry_index(entries, filepath) != -1)
        return -1;  // fil finns redan

    // 3. Läs in data från användaren
    std::string data;
    std::string line;
    while (true)
    {
        std::getline(std::cin, line);
        if (line.empty())
            break;
        data += line;
        data += '\n';
    }

    int file_size = data.size();

    // 4. Fall: tom fil
    int first_block = FAT_EOF;
    if (file_size == 0)
    {
        first_block = FAT_EOF;
    }
    else
    {
        // 5. Dela upp i block och skriv
        int pos = 0;
        int prev_block = -1;

        while (pos < file_size)
        {
            int block = find_free_block();
            if (block == -1)
                return -2; // slut på disk

            if (first_block == FAT_EOF)
                first_block = block;

            if (prev_block != -1)
                fat[prev_block] = block;

            prev_block = block;

            uint8_t buffer[BLOCK_SIZE];
            std::memset(buffer, 0, BLOCK_SIZE);

            int chunk = std::min((int)BLOCK_SIZE, file_size - pos);
            std::memcpy(buffer, data.data() + pos, chunk);
            pos += chunk;

            disk.write(block, buffer);
        }

        fat[prev_block] = FAT_EOF;
    }

    // 6. Spara FAT
    save_fat();

    // 7. Skapa katalogpost
    int free_idx = find_free_dir_entry(entries);
    if (free_idx == -1)
        return -3; // katalogen full

    dir_entry &e = entries[free_idx];

    std::memset(&e, 0, sizeof(dir_entry));
    std::strncpy(e.file_name, filepath.c_str(), sizeof(e.file_name)-1);
    e.size = file_size;
    e.first_blk = (first_block == FAT_EOF ? FAT_EOF : first_block);
    e.type = TYPE_FILE;
    e.access_rights = READ | WRITE; // rw-

    // 8. Skriv katalogen till disk
    write_dir(current_dir, entries);

    return 0;
}



// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    // 1. Läs katalogen
    dir_entry entries[BLOCK_SIZE / sizeof(dir_entry)];
    read_dir(current_dir, entries);

    // 2. Hitta filen
    int idx = find_dir_entry_index(entries, filepath);
    if (idx == -1)
        return -1; // filen finns inte

    dir_entry &e = entries[idx];

    if (e.type != TYPE_FILE)
        return -2; // är en directory

    // 3. Läs blocken i FAT-kedjan
    uint16_t block = e.first_blk;
    int remaining = e.size;

    while (block != FAT_EOF && remaining > 0)
    {
        uint8_t buffer[BLOCK_SIZE];
        disk.read(block, buffer);

        int to_print = std::min((int)BLOCK_SIZE, remaining);
        std::cout.write((char*)buffer, to_print);

        remaining -= to_print;
        block = fat[block];
    }

    return 0;
}


// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    dir_entry entries[BLOCK_SIZE / sizeof(dir_entry)];
    read_dir(current_dir, entries);

    std::cout << "name size\n";

    int max = BLOCK_SIZE / sizeof(dir_entry);
    for (int i = 0; i < max; i++)
    {
        if (entries[i].file_name[0] == '\0')
            continue;

        std::cout << entries[i].file_name << " "
                  << entries[i].size << "\n";
    }

    return 0;
}


// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
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

int FS::load_fat()
{
    uint8_t buffer[BLOCK_SIZE];
    disk.read(FAT_BLOCK, buffer);
    std::memcpy(fat, buffer, BLOCK_SIZE);
    return 0;
}

int FS::save_fat()
{
    uint8_t buffer[BLOCK_SIZE];
    std::memcpy(buffer, fat, BLOCK_SIZE);
    disk.write(FAT_BLOCK, buffer);
    return 0;
}

int FS::read_dir(uint16_t block, dir_entry *entries)
{
    uint8_t buffer[BLOCK_SIZE];
    disk.read(block, buffer);
    std::memcpy(entries, buffer, BLOCK_SIZE);
    return 0;
}

int FS::write_dir(uint16_t block, dir_entry *entries)
{
    uint8_t buffer[BLOCK_SIZE];
    std::memcpy(buffer, entries, BLOCK_SIZE);
    disk.write(block, buffer);
    return 0;
}

int FS::find_free_block()
{
    for (int i = 0; i < BLOCK_SIZE/2; i++)
        if (fat[i] == FAT_FREE)
            return i;
    return -1;
}

int FS::find_free_dir_entry(dir_entry *entries)
{
    int max = BLOCK_SIZE / sizeof(dir_entry);
    for (int i = 0; i < max; i++)
        if (entries[i].file_name[0] == '\0')
            return i;
    return -1;
}

int FS::find_dir_entry_index(dir_entry *entries, const std::string &name)
{
    int max = BLOCK_SIZE / sizeof(dir_entry);
    for (int i = 0; i < max; i++)
        if (name == entries[i].file_name)
            return i;
    return -1;
}

