# Virtual File System Simulator

A simplified virtual file system that runs on top of a virtual disk image. Supports file creation, reading, writing, and deletion using block-based storage, a free-space bitmap, an inode table, and SSTF disk scheduling.

## Compile

```bash
gcc fs.c -o fs -Wextra -Wall
```

## Run

```bash
./fs -f <disk_file> -d <disk_size> -b <block_size> -t <total_blocks> -w <workload_file>
```

### Example

```bash
./fs -f vdisk.img -d 1048576 -b 512 -t 2048 -w workload.txt
```

## Command Line Options

| Flag | Description |
|------|-------------|
| `-f` | Name of the virtual disk image file |
| `-d` | Total disk size in bytes |
| `-b` | Block size in bytes |
| `-t` | Total number of blocks (`disk_size / block_size`) |
| `-w` | Workload file containing commands to execute |

> Note: `disk_size` must equal `block_size * total_blocks`.

## Disk Layout

| Blocks   | Purpose          |
|----------|------------------|
| Block 0  | Superblock       |
| Blocks 1–4  | Free-space bitmap |
| Blocks 5–20 | Inode table      |
| Blocks 21+  | File data        |

## Workload File Commands

Each line in the workload file contains one command:

| Command | Description |
|---------|-------------|
| `format` | Initialize the file system |
| `create <filename>` | Create a new empty file |
| `write <filename> <data>` | Write data to a file (contiguous block allocation) |
| `read <filename>` | Read and print a file's contents |
| `delete <filename>` | Delete a file and free its blocks |
| `ls` | List all files and their sizes |
| `stat <filename>` | Display file metadata (size, start block, block count) |
| `bitmap` | Display the free-space bitmap |

### Example Workload File

```
format
create file1.txt
write file1.txt hello world
read file1.txt
stat file1.txt
ls
bitmap
delete file1.txt
```

## Example Output

```
Disk formatted successfully.
File created: file1.txt
Data written to file: file1.txt (1 block(s))
SSTF Service Order:
21
Total Head Movement: 21
Data read from file file1.txt: hello world
SSTF Service Order:
21
Total Head Movement: 21
File: file1.txt
Size: 11 bytes
Start Block: 21
Block Count: 1
Files on disk:
- file1.txt (size: 11 bytes)
Free-space Bitmap (1=allocated, 0=free):
1111111111111111111110000000000000000000000000000000000000000000
...
File deleted: file1.txt
```
