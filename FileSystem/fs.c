/*
Will Baca
CPSC380-01
File System Simulation
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

typedef struct
{
    int magic_number;
    int disk_size;
    int block_size;
    int total_blocks;
    int bitmap_start;
    int bitmap_blocks;
    int inode_start;
    int inode_blocks;
    int data_start;
} superblock_t;

typedef struct
{
    char filename[32];
    int size;
    int start_block;
    int block_count;
    int used;
} inode_t;

typedef struct
{
    FILE *disk_file;
    superblock_t superblock;
    unsigned char *bitmap;
    inode_t *inode_table;
} virtual_disk_t;

void initialize_superblock(virtual_disk_t *vd, int disk_size, int block_size, int total_blocks);
void initialize_bitmap(virtual_disk_t *vd);
void initialize_inode_table(virtual_disk_t *vd);
void flush_inode_table(virtual_disk_t *vd);
void flush_bitmap(virtual_disk_t *vd);

void format_disk(virtual_disk_t *vd);

void disk_create(virtual_disk_t *vd, const char *filename, int disk_size, int block_size, int total_blocks);
void disk_open(virtual_disk_t *vd, const char *filename);
void disk_close(virtual_disk_t *vd);
void disk_read_block(virtual_disk_t *vd, int block_number, void *buffer);
void disk_write_block(virtual_disk_t *vd, int block_number, const void *buffer);

void cmd_create(virtual_disk_t *vd, const char *filename);
void cmd_write(virtual_disk_t *vd, const char *filename, const char *data);
void cmd_read(virtual_disk_t *vd, const char *filename);
void cmd_delete(virtual_disk_t *vd, const char *filename);
void cmd_ls(virtual_disk_t *vd);
void cmd_stat(virtual_disk_t *vd, const char *filename);
void cmd_bitmap(virtual_disk_t *vd);
void cmd_format(virtual_disk_t *vd);

int process_workload(virtual_disk_t *vd, const char *workload_filename);
int sstf_schedule(int *accessed_blocks, int num_blocks, int initial_head_position, int *service_order);

void initialize_superblock(virtual_disk_t *vd, int disk_size, int block_size, int total_blocks)
{
    vd->superblock.magic_number = 0x12345678; // Example magic number
    vd->superblock.disk_size = disk_size;
    vd->superblock.block_size = block_size;
    vd->superblock.total_blocks = total_blocks;
    vd->superblock.bitmap_start = 1; // Bitmap starts at block 1
    vd->superblock.bitmap_blocks = 4; // Bitmap occupies blocks 1-4
    vd->superblock.inode_start = 5; // Inode table starts at block 5
    vd->superblock.inode_blocks = 16; // Inode table occupies blocks 5-20
    vd->superblock.data_start = 21; // Data blocks start at block 21
}

void initialize_bitmap(virtual_disk_t *vd)
{
    int bitmap_size = vd->superblock.bitmap_blocks * vd->superblock.block_size;
    vd->bitmap = (unsigned char *)malloc(bitmap_size);
    memset(vd->bitmap, 0, bitmap_size); // Mark all blocks as free

    // Mark metadata blocks as allocated
    for (int i = 0; i < vd->superblock.data_start; i++)
    {
        int byte_index = i / 8;
        int bit_index = i % 8;
        vd->bitmap[byte_index] |= (1 << bit_index); // Mark block as allocated
    }
}

void initialize_inode_table(virtual_disk_t *vd)
{
    int inode_table_size = vd->superblock.inode_blocks * vd->superblock.block_size;
    vd->inode_table = (inode_t *)malloc(inode_table_size);
    memset(vd->inode_table, 0, inode_table_size); // Clear inode table
}

void flush_inode_table(virtual_disk_t *vd)
{
    unsigned char *raw = (unsigned char *)vd->inode_table;
    for(int i = 0; i < vd->superblock.inode_blocks; i++)
    {
        disk_write_block(vd, vd->superblock.inode_start + i, raw + (i * vd->superblock.block_size));
    }
}

// Write the bitmap to disk
void flush_bitmap(virtual_disk_t *vd)
{
    unsigned char *raw = vd->bitmap;
    for(int i = 0; i < vd->superblock.bitmap_blocks; i++)
    {
        disk_write_block(vd, vd->superblock.bitmap_start + i, raw + (i * vd->superblock.block_size));
    }
}

// Format the disk by clearing the bitmap and inode table, and marking metadata blocks as allocated
void format_disk(virtual_disk_t *vd)
{
    // Clear bitmap and inode table
    memset(vd->bitmap, 0, vd->superblock.bitmap_blocks * vd->superblock.block_size);
    memset(vd->inode_table, 0, vd->superblock.inode_blocks * vd->superblock.block_size);

    // Mark metadata blocks as allocated in bitmap
    for (int i = 0; i < vd->superblock.data_start; i++)
    {
        int byte_index = i / 8;
        int bit_index = i % 8;
        vd->bitmap[byte_index] |= (1 << bit_index); // Mark block as allocated
    }

    // Write superblock to disk
    disk_write_block(vd, 0, &vd->superblock);
    // Write bitmap and inode table to disk
    flush_bitmap(vd);
    flush_inode_table(vd);
}

// Create a new disk file with the specified parameters and initialize the superblock, bitmap, and inode table
void disk_create(virtual_disk_t *vd, const char *filename, int disk_size, int block_size, int total_blocks)
{
    vd->disk_file = fopen(filename, "wb+");
    if (vd->disk_file == NULL)
    {
        fprintf(stderr, "Error creating disk file: %s\n", filename);
        exit(EXIT_FAILURE);
    }
    fseek(vd->disk_file, disk_size - 1, SEEK_SET);
    fputc('\0', vd->disk_file); // Allocate disk space

    initialize_superblock(vd, disk_size, block_size, total_blocks);
    initialize_bitmap(vd);
    initialize_inode_table(vd);

    // Write superblock to disk
    disk_write_block(vd, 0, &vd->superblock);
}

// Open an existing disk file, read the superblock, bitmap, and inode table into memory, and validate the disk file
void disk_open(virtual_disk_t *vd, const char *filename)
{
    int bmap_size, inode_table_size;
    vd->disk_file = fopen(filename, "rb+");
    if (vd->disk_file == NULL)
    {
        fprintf(stderr, "Error opening disk file: %s\n", filename);
        exit(EXIT_FAILURE);
    }
    // Read superblock from disk
    fread(&vd->superblock, sizeof(superblock_t), 1, vd->disk_file);
    // Validate superblock
    if (vd->superblock.magic_number != 0x12345678)
    {
        fprintf(stderr, "Invalid disk file: %s\n", filename);
        fclose(vd->disk_file);
        exit(EXIT_FAILURE);
    }
    // Read superblock from disk
    bmap_size = vd->superblock.bitmap_blocks * vd->superblock.block_size;
    vd->bitmap = (unsigned char *)malloc(bmap_size);
    for(int i = 0; i < vd->superblock.bitmap_blocks; i++)
    {
        disk_read_block(vd, vd->superblock.bitmap_start + i, vd->bitmap + (i * vd->superblock.block_size));
    }

    inode_table_size = vd->superblock.inode_blocks * vd->superblock.block_size;
    vd->inode_table = (inode_t *)malloc(inode_table_size);
    for(int i = 0; i < vd->superblock.inode_blocks; i++)
    {
        disk_read_block(vd, vd->superblock.inode_start + i, (unsigned char *)vd->inode_table + (i * vd->superblock.block_size));
    }
}

// Close the disk file and free any allocated memory for the bitmap and inode table
void disk_close(virtual_disk_t *vd)
{
    if (vd->disk_file)
    {
        fclose(vd->disk_file);
        vd->disk_file = NULL;
    }
    if (vd->bitmap)
    {
        free(vd->bitmap);
        vd->bitmap = NULL;
    }
    if (vd->inode_table)
    {
        free(vd->inode_table);
        vd->inode_table = NULL;
    }
}


void disk_read_block(virtual_disk_t *vd, int block_number, void *buffer)
{
    fseek(vd->disk_file, block_number * vd->superblock.block_size, SEEK_SET);
    fread(buffer, (size_t)vd->superblock.block_size, 1, vd->disk_file);
}

void disk_write_block(virtual_disk_t *vd, int block_number, const void *buffer)
{
    fseek(vd->disk_file, block_number * vd->superblock.block_size, SEEK_SET);
    fwrite(buffer, (size_t)(vd->superblock.block_size), 1, vd->disk_file);
}

// Create a new file by finding a free inode, initializing it with the filename and metadata, and writing the updated inode table to disk. If the file already exists or no free inodes are available, print an error message.
void cmd_create(virtual_disk_t *vd, const char *filename)
{
    int num_inodes = vd->superblock.inode_blocks * vd->superblock.block_size / (int)sizeof(inode_t);
    // find free inode
    for (int i = 0; i < num_inodes; i++)
    {
        if (vd->inode_table[i].used && strcmp(vd->inode_table[i].filename, filename) == 0)
        {
            fprintf(stderr, "File already exists: %s\n", filename);
            return;
        }
    }
    for (int i = 0; i < num_inodes; i++)
    {
        if (!vd->inode_table[i].used)
        {
            strncpy(vd->inode_table[i].filename, filename, sizeof(vd->inode_table[i].filename) - 1);
            vd->inode_table[i].filename[sizeof(vd->inode_table[i].filename) - 1] = '\0';
            vd->inode_table[i].size = 0;
            vd->inode_table[i].start_block = 0;
            vd->inode_table[i].block_count = 0;
            vd->inode_table[i].used = 1;
            // Write updated inode table to disk
            flush_inode_table(vd);
            printf("File created: %s\n", filename);
            return;
        }
    }
    fprintf(stderr, "No free inodes available to create file: %s\n", filename);
}

// Write data to a file by finding:
// the corresponding inode, 
// calculating the number of blocks needed, 
// finding free blocks in the bitmap, 
// writing the data to those blocks, 
// updating the inode metadata, 
// writing the updated bitmap and inode table to disk.
void cmd_write(virtual_disk_t *vd, const char *filename, const char *data)
{
    int num_nodes = vd->superblock.inode_blocks * vd->superblock.block_size / (int)sizeof(inode_t);
    int data_len, blocks_needed, start_block;
    for (int i = 0; i < num_nodes; i++)
    {
        // Find the inode for the file
        if (vd->inode_table[i].used && strcmp(vd->inode_table[i].filename, filename) == 0)
        {
            data_len = (int)strlen(data);
            blocks_needed = (data_len + vd->superblock.block_size - 1) / vd->superblock.block_size;
            start_block = -1;
            // Find free blocks in the bitmap
            for (int j = vd->superblock.data_start; j <= vd->superblock.total_blocks - blocks_needed; j++)
            {
                int found = 1;
                for(int k = 0; k < blocks_needed; k++)
                {
                    if (vd->bitmap[(j + k) / 8] & (1 << ((j + k) % 8))) // Check if block is allocated
                    {
                        found = 0;
                        break;
                    }
                }
                if(found)
                {
                    start_block = j;
                    break;
                }
            }
            
            if (start_block == -1)
            {
                fprintf(stderr, "Disk full\n");
                return;
            }
            for (int b = 0; b < blocks_needed; b++)
            {
                char block_buf[vd->superblock.block_size];
                memset(block_buf, 0, vd->superblock.block_size);
                strncpy(block_buf, data + b * vd->superblock.block_size, vd->superblock.block_size - 1);
                disk_write_block(vd, start_block + b, block_buf);
                // Mark the block as allocated in the bitmap
                vd->bitmap[(start_block + b) / 8] |= (1 << ((start_block + b) % 8)); // Mark block as allocated
            }

            // Update inode metadata
            vd->inode_table[i].size = data_len;
            vd->inode_table[i].start_block = start_block;
            vd->inode_table[i].block_count = blocks_needed;
            // Write updated inode table and bitmap to disk
            flush_bitmap(vd);
            flush_inode_table(vd);
            printf("Data written to file: %s (%d block(s))\n", filename, blocks_needed);

            int blocks[blocks_needed];
            int order[blocks_needed];
            for(int k = 0; k < blocks_needed; k++)
            {
                blocks[k] = start_block + k;
            }
            int movement = sstf_schedule(blocks, blocks_needed, 0, order); // Assuming initial head position is 0
            printf("SSTF Service Order:\n");
            for(int j = 0; j < blocks_needed; j++)
            {
                printf("%d%s", order[j], (j < blocks_needed - 1) ? " -> " : "\n");
            }
            printf("Total Head Movement: %d\n", movement);
            return;
        }
    }
    fprintf(stderr, "File not found: %s\n", filename);
}

// Read data from a file by finding the corresponding inode, 
// reading the data blocks into a buffer, 
// and printing the data to the console. If
// the file is not found, print an error message.
void cmd_read(virtual_disk_t *vd, const char *filename)
{
    int num_inodes = vd->superblock.inode_blocks * vd->superblock.block_size / (int)sizeof(inode_t);
    int movement;
    char *buf;
    for (int i = 0; i < num_inodes; i++)
    {
        if (vd->inode_table[i].used && strcmp(vd->inode_table[i].filename, filename) == 0)
        {
            if(vd->inode_table[i].block_count == 0)
            {
                fprintf(stderr, "File %s is empty\n", filename);
                return;
            }
            int count = vd->inode_table[i].block_count;
            int blocks[count], order[count];
            buf = calloc((size_t)(count * vd->superblock.block_size + 1), 1);
            for (int k = 0; k < count; k++)
            {
                disk_read_block(vd, vd->inode_table[i].start_block + k, buf + k * vd->superblock.block_size);
                blocks[k] = vd->inode_table[i].start_block + k;
            }
            buf[vd->inode_table[i].size] = '\0'; // Null-terminate the data
            printf("Data read from file %s: %s\n", filename, buf);
            free(buf);

            movement = sstf_schedule(blocks, count, 0, order); // Assuming initial head position is 0
            printf("SSTF Service Order:\n");
            for(int j = 0; j < count; j++)
            {
                printf("%d%s", order[j], (j < count - 1) ? " -> " : "\n");
            }
            printf("Total Head Movement: %d\n", movement);
            return;
        }
    }
    fprintf(stderr, "File not found: %s\n", filename);
}


void cmd_delete(virtual_disk_t *vd, const char *filename)
{
    // Find the inode for the file
    int num_inodes = vd->superblock.inode_blocks * vd->superblock.block_size / (int)sizeof(inode_t);
    int data_block, byte_index, bit_index;
    for (int i = 0; i < num_inodes; i++)
    {
        if (vd->inode_table[i].used && strcmp(vd->inode_table[i].filename, filename) == 0)
        {
            // Mark the inode as unused
            vd->inode_table[i].used = 0;
            // Mark the data block as free in the bitmap
            data_block = vd->inode_table[i].start_block;
            byte_index = data_block / 8;
            bit_index = data_block % 8;
            vd->bitmap[byte_index] &= ~(1 << bit_index); // Mark block as free
            // Write updated inode table and bitmap to disk
            flush_bitmap(vd);
            flush_inode_table(vd);
            printf("File deleted: %s\n", filename);
            return;
        }
    }
    fprintf(stderr, "File not found: %s\n", filename);
}

void cmd_ls(virtual_disk_t *vd)
{
    int num_inodes = vd->superblock.inode_blocks * vd->superblock.block_size / (int)sizeof(inode_t);
    printf("Files on disk:\n");
    for (int i = 0; i < num_inodes; i++)
    {
        if (vd->inode_table[i].used)
        {
            printf("- %s (size: %d bytes)\n", vd->inode_table[i].filename, vd->inode_table[i].size);
        }
    }
}

void cmd_stat(virtual_disk_t *vd, const char *filename)
{
    int num_inodes = vd->superblock.inode_blocks * vd->superblock.block_size / (int)sizeof(inode_t);
    for (int i = 0; i < num_inodes; i++)
    {
        if (vd->inode_table[i].used && strcmp(vd->inode_table[i].filename, filename) == 0)
        {
            printf("File: %s\n- Size: %d bytes\n- Start Block: %d\n- Block Count: %d\n", vd->inode_table[i].filename, vd->inode_table[i].size, vd->inode_table[i].start_block, vd->inode_table[i].block_count);
            return;
        }
    }
    fprintf(stderr, "File not found: %s\n", filename);
}

void cmd_bitmap(virtual_disk_t *vd)
{
    printf("Free-space Bitmap (1 = allocated, 0 = free):\n");
    for (int i = 0; i < vd->superblock.total_blocks; i++)
    {
        printf("%d", (vd->bitmap[i/8] >> (i % 8)) & 1);
        if ((i + 1) % 64 == 0) // Print a newline every 64 blocks for readability
        {
            printf("\n");
        }
    }
    printf("\n");
}

void cmd_format(virtual_disk_t *vd)
{
    format_disk(vd);
    printf("Disk formatted successfully.\n");
}

int process_workload(virtual_disk_t *vd, const char *workload_filename)
{
    char line[8192];
    FILE *workload_file = fopen(workload_filename, "r");
    if (workload_file == NULL)
    {
        fprintf(stderr, "Error opening workload file: %s\n", workload_filename);
        return -1;
    }
    while (fgets(line, sizeof(line), workload_file))
    {
        char *command = strtok(line, " \n");
        if (command == NULL)
            continue;

        if (strcmp(command, "format") == 0)
        {
            cmd_format(vd);
        }
        else if (strcmp(command, "create") == 0)
        {
            char *filename = strtok(NULL, " \n");
            if (filename)
                cmd_create(vd, filename);
        }
        else if (strcmp(command, "write") == 0)
        {
            char *filename = strtok(NULL, " \n");
            char *data = strtok(NULL, "\n");
            if (filename && data)
                cmd_write(vd, filename, data);
        }
        else if (strcmp(command, "read") == 0)
        {
            char *filename = strtok(NULL, " \n");
            if (filename)
                cmd_read(vd, filename);
        }
        else if (strcmp(command, "delete") == 0)
        {
            char *filename = strtok(NULL, " \n");
            if (filename)
                cmd_delete(vd, filename);
        }
        else if (strcmp(command, "ls") == 0)
        {
            cmd_ls(vd);
        }
        else if (strcmp(command, "stat") == 0)
        {
            char *filename = strtok(NULL, " \n");
            if (filename)
                cmd_stat(vd, filename);
        }
        else if (strcmp(command, "bitmap") == 0)
        {
            cmd_bitmap(vd);
        }
    }
    fclose(workload_file);
    return 0;
}

int sstf_schedule(int *accessed_blocks, int num_blocks, int initial_head_position, int *service_order)
{
    int total_head_movement = 0;
    int serviced[num_blocks];

    for (int i = 0; i < num_blocks; i++)
    {
        serviced[i] = 0; // Mark all requests as unserviced
    }

    for(int i = 0; i < num_blocks; i++)
    {
        int min_distance = __INT_MAX__;
        int next_request_index = -1;

        // Find the unserviced request closest to the current head position
        for (int j = 0; j < num_blocks; j++)
        {
            if (!serviced[j])
            {
                int distance = abs(initial_head_position - accessed_blocks[j]);
                if (distance < min_distance)
                {
                    min_distance = distance;
                    next_request_index = j;
                }
            }
        }

        // Service the closest request
        serviced[next_request_index] = 1;
        service_order[i] = accessed_blocks[next_request_index];
        initial_head_position = accessed_blocks[next_request_index]; // Move the head to the serviced request
        total_head_movement += min_distance;
    }
    return total_head_movement;
}

int main(int argc, char* argv[])
{
    int option;
    char disk_filename[256] = "";
    char workload_filename[256] = "";
    int disk_size = 0;
    int block_size = 0;
    int total_blocks = 0;
    virtual_disk_t vd;

    // Define long options
    static struct option long_options[] =
    {
        {"file", required_argument, 0, 'f'},
        {"disk_size", required_argument, 0, 'd'},
        {"block_size", required_argument, 0, 'b'},
        {"total_blocks", required_argument, 0, 't'},
        {"workload", required_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    while ((option = getopt_long(argc, argv, "f:d:b:t:w:", long_options, NULL)) != -1)
    {
        switch (option)
        {
            case 'f':
                strncpy(disk_filename, optarg, sizeof(disk_filename) - 1);
                disk_filename[sizeof(disk_filename) - 1] = '\0';
                break;
            case 'd':
                disk_size = atoi(optarg);
                break;
            case 'b':
                block_size = atoi(optarg);
                break;
            case 't':
                total_blocks = atoi(optarg);
                break;
            case 'w':
                strncpy(workload_filename, optarg, sizeof(workload_filename) - 1);
                workload_filename[sizeof(workload_filename) - 1] = '\0';
                break;
            default:
                fprintf(stderr, "Usage: %s -f <disk_file> -d <disk_size> -b <block_size> -t <total_blocks> -w <workload_file>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    // Validate command-line arguments
    if (disk_size <= 0 || block_size <= 0 || total_blocks <= 0)
    {
        fprintf(stderr, "Invalid disk parameters. Please provide positive integers for disk size, block size, and total blocks.\n");
        exit(EXIT_FAILURE);
    }
    if (strlen(disk_filename) == 0)
    {
        fprintf(stderr, "Disk filename is required. Use -f <disk_file> to specify the disk file.\n");
        exit(EXIT_FAILURE);
    }
    if (disk_size != block_size * total_blocks)
    {
        fprintf(stderr, "Disk size must be equal to block size multiplied by total blocks.\n");
        exit(EXIT_FAILURE);
    }


    // Check if a disk already exists
    // If it does exist open the existing disk 
    // Otherwise create a new disk with the specified parameters
    if (access(disk_filename, F_OK) == 0)
        disk_open(&vd, disk_filename);
    else
        disk_create(&vd, disk_filename, disk_size, block_size, total_blocks);
    
    printf("Disk initialized: %s (size: %d bytes, block size: %d bytes, total blocks: %d)\n", disk_filename, disk_size, block_size, total_blocks);
    
    process_workload(&vd, workload_filename);

    disk_close(&vd);
    return 0;
}

