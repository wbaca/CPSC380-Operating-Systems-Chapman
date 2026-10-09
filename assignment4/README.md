# Virtual Memory Manager

## Overview

The task here was to create a program that simulates a virtual memory manager. The program translates logical addresses to physical addresses for a virtual address space of $2^{16}$ (65,536 bytes). By reading a file containing logical addresses and using a TLB and page table, translating each address to its corresponding physical address and outputting the value of the byte stored at the translated physical address. We want to develop the understanding of the steps involved in address translation. We are to implement a way to resolve page faults using demand paging, managing a TLB, implementing multiple page-replacement algorithms, and analyzing how memory access differs across the management methods. 

---

## How to Compile
Ensure you have `gcc` installed.

```bash
gcc vmmgr.c -o vmmgr -Wextra -Wall
```

## How to Run

```bash
./vmmgr <address_file> --t <tlb_policy> --p <page_policy> [--f <frames>] [--verbose]
```

All three of `-t`, `-p`, and the address file are required. `-f` defaults to 128 if omitted.

## CLI Options

| Flag  | Values | Description |
|-------|--------|-------------|
| `--t` | `lru`, `random` | TLB replacement policy |
| `--p` | `fifo`, `lru`, `random` | Page replacement policy |
| `--f` | `1–256` | Number of physical frames (default: 128) |
| `--v` | — | Print TLB hit/miss and page fault details per access |
| `-h` | — | Print usage help |

##### Examples:

```bash
# LRU TLB, FIFO page replacement, 128 frames
./vmmgr addresses.txt -t lru -p fifo -f 128

# Random TLB, LRU page replacement, 256 frames with verbose output
./vmmgr addresses.txt -t random -p lru -f 256 --verbose
```

## Input File Format

A plain text file with one logical address (32-bit integer) per line. Only the rightmost 16 bits are used.

##### Example file:

```
16916
62493
30198
53683
40185
```

## Output

For each address, the program prints:
```
Virtual Address: <n>, Physical Address: <n>, Value: <n>
```
Example:
```
Virtual Address: 2301, Physical Address: 35325, Value: 0
Virtual Address: 7736, Physical Address: 57912, Value: 0
Virtual Address: 31260, Physical Address: 23324, Value: 0
Virtual Address: 17071, Physical Address: 175, Value: -85
Virtual Address: 8940, Physical Address: 46572, Value: 0
Virtual Address: 9929, Physical Address: 44745, Value: 0
Virtual Address: 45563, Physical Address: 46075, Value: 126
Virtual Address: 12107, Physical Address: 2635, Value: -46
```
A CSV file (`results.csv`) is also generated with one rows:
```
Logical Address, Page Number, Offset, TLB Hit, Page Fault, Frame Number, Physical Address, Value at Address, Replaced Page/Frame
```

An example of final statistics printed to stdout using the random addresses file, table LRU, page LRU, and 128-page frames:
```
Total addresses: 1000
Page faults: 537
Page fault rate: 53.70
TLB hits: 55
TLB misses: 945
TLB hit rate: 5.50%
```

## Data Analysis Report

After running the program with all 12 different variations of options, lru|fifo|128, lru|lru|128, etc. I noticed that the address file made the most noticeable differences across each test. The policy that performed best under high locality was LRU because it keeps the pages that were recently used in memory, FIFO and random didn't do as well because they don't have locality to exploit. Based on that LRU outperforms FIFO when there is temporal locality, the TLB is small, so the different TLB algorithms don't have much difference. The fine with looping addresses shows this because the addresses have more locality. Random page replacement is similar to other policies when there is low locality, like the random address file, and where there is large frame size. With no reuse pattern to exploit when using random addresses the page replacement algorithms end up with similar amounts of page faults. With smaller memory there are more page faults because there can be only so many pages in the frames allocated. 

### Conclusion

After creating this program I can now see how important it is when choosing page or frame size and which replacement policy to use when considering memory management. I can imagine much more goes into the systems we use today to make them as fast and responsive as possible.   