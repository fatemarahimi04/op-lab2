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


int FS::create(std::string filepath)
{
    dir_entry entries[BLOCK_SIZE / sizeof(dir_entry)];
    read_dir(current_dir, entries);

    if (find_dir_entry_index(entries, filepath) != -1)
        return -1;

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

    int first_block = FAT_EOF;
    if (file_size == 0)
    {
        first_block = FAT_EOF;
    }
    else
    {
        int pos = 0;
        int prev_block = -1;

        while (pos < file_size)
        {
            int block = find_free_block();
            if (block == -1)
                return -2;

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

    save_fat();

    int free_idx = find_free_dir_entry(entries);
    if (free_idx == -1)
        return -3;

    dir_entry &e = entries[free_idx];

    std::memset(&e, 0, sizeof(dir_entry));
    std::strncpy(e.file_name, filepath.c_str(), sizeof(e.file_name)-1);
    e.size = file_size;
    e.first_blk = (first_block == FAT_EOF ? FAT_EOF : first_block);
    e.type = TYPE_FILE;
    e.access_rights = READ | WRITE;

    write_dir(current_dir, entries);

    return 0;
}



int FS::cat(std::string filepath)
{
    dir_entry entries[BLOCK_SIZE / sizeof(dir_entry)];
    read_dir(current_dir, entries);

    int idx = find_dir_entry_index(entries, filepath);
    if (idx == -1)
        return -1;

    dir_entry &e = entries[idx];

    if (e.type != TYPE_FILE)
        return -2;

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


int
FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

int
FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

int
FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

int
FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

int
FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

int
FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

int
FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

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

