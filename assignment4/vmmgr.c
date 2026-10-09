/*
Will Baca
CPSC380-01
Virtual Memory Management
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>

#define PAGE_SIZE 256
#define TLB_SIZE 16
#define NUM_PAGES 256
 
static char *physical_memory = NULL;
static int *frame_queue = NULL;

typedef struct {
    int page_number;
    int frame_number;
    int valid;
    int access_time; // For LRU policy
} TLBEntry;

typedef struct {
    int frame_number;
    int valid;
    int access_time; // For LRU policy
} PageTableEntry;


static TLBEntry tlb[TLB_SIZE];

static PageTableEntry page_table[NUM_PAGES];

static int verbose_flag = 0;

static int num_frames = 128; // Default number of frames, can be set via command-line argument
static int total_addresses = 0;
static int pg_faults = 0;
static int tlb_hits = 0;
static int tlb_misses = 0;
static int last_replaced_page = -1;

static int current_time = 0;

static int queue_front = 0;
static int queue_rear = 0;
static int frame_count = 0;

static int (*get_frame)(void);
static void (*update_tlb)(int page_number, int frame_number);
static void (*update_page_table)(int page_number, int frame_number);

void init_tlb(void);
void init_page_table(void);
int is_tlb(int page_number, int *frame_number);
int page_table_check(int page_number, int *frame_number);

void update_tlb_lru(int page_number, int frame_number);
void update_tlb_random(int page_number, int frame_number);

void update_page_table_fifo(int page_number, int frame_number);
void update_page_table_lru(int page_number, int frame_number);
void update_page_table_random(int page_number, int frame_number);

int get_frame_fifo(void);
int get_frame_lru(void);
int get_frame_random(void);

int translate_address(int logical_address, int *physical_address, FILE *backing_store, int *out_page, int *out_offset, int *out_tlb_hit, int *out_page_fault, int *out_frame);
int page_fault_handler(int page_number, FILE *backing_store);
void load_page_into_memory(int page_number, int frame_number, FILE *backing_store);

void print_stats(void);
void write_csv_row(FILE *csv, int logical_address, int page_number, int offset, int tlb_hit, int page_fault, int frame_number, int physical_address, char value_at_address, int replaced_page_frame);

void init_tlb(void) {
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = 0;
        tlb[i].access_time = 0;
        tlb[i].frame_number = -1;
        tlb[i].page_number = -1;
    }
}
void init_page_table(void) {
    for (int i = 0; i < NUM_PAGES; i++) {
        page_table[i].valid = 0;
        page_table[i].frame_number = -1;
    }
}

// Check if page number is in TLB, return 1 if hit and set frame_number, otherwise return 0 for miss
int is_tlb(int page_number, int *frame_number)
{
    for (int i = 0; i < TLB_SIZE; i++)
    {
        if (tlb[i].valid && tlb[i].page_number == page_number)
        {
            *frame_number = tlb[i].frame_number;
            tlb[i].access_time = current_time++;
            tlb_hits++;
            return 1; // TLB hit
        }
    }
    tlb_misses++;
    return 0; // TLB miss
}

int page_table_check(int page_number, int *frame_number)
{
    if (page_table[page_number].valid)
    {
        *frame_number = page_table[page_number].frame_number;
        page_table[page_number].access_time = current_time++;
        return 1; // Page table hit
    }
    if (page_number >= NUM_PAGES || page_number < 0)
    {
        fprintf(stderr, "Invalid page number: %d\n", page_number);
        exit(EXIT_FAILURE);
    }
    return 0;
}

// Update TLB with new page number and frame number, using LRU replacement policy
void update_tlb_lru(int page_number, int frame_number)
{
    // Find an invalid entry or the least recently used entry to replace
    int lru_index = 0;
    for (int i = 0; i < TLB_SIZE; i++)
    {
        // If we find an invalid entry, use it for the new page
        if (!tlb[i].valid)
        {
            tlb[i].valid = 1;
            tlb[i].page_number = page_number;
            tlb[i].frame_number = frame_number;
            tlb[i].access_time = current_time++;
            return;
        }
        // Find the least recently used entry
        if (tlb[i].access_time < tlb[lru_index].access_time)
        {
            lru_index = i;
        }
    }
    // Replace the least recently used entry
    tlb[lru_index].page_number = page_number;
    tlb[lru_index].frame_number = frame_number;
    tlb[lru_index].access_time = current_time++;
}

// Update TLB with new page number and frame number, using Random replacement policy
void update_tlb_random(int page_number, int frame_number)
{
    // Find an invalid entry to replace
    for (int i = 0; i < TLB_SIZE; i++)
    {
        // If we find an invalid entry, use it for the new page
        if (!tlb[i].valid)
        {
            tlb[i].valid = 1;
            tlb[i].page_number = page_number;
            tlb[i].frame_number = frame_number;
            return;
        }
    }
    // Replace a random entry
    int random_index = rand() % TLB_SIZE;
    tlb[random_index].page_number = page_number;
    tlb[random_index].frame_number = frame_number;
}

// Update page table with new page number and frame number, using FIFO replacement policy
void update_page_table_fifo(int page_number, int frame_number)
{
    page_table[page_number].valid = 1;
    page_table[page_number].frame_number = frame_number;
}

// Update page table with new page number and frame number, using LRU replacement policy
void update_page_table_lru(int page_number, int frame_number)
{
    page_table[page_number].valid = 1;
    page_table[page_number].frame_number = frame_number;
    page_table[page_number].access_time = current_time++;
}

// Update page table with new page number and frame number, using Random replacement policy
void update_page_table_random(int page_number, int frame_number)
{
    page_table[page_number].valid = 1;
    page_table[page_number].frame_number = frame_number;
}

int get_frame_fifo(void)
{
    // Return the next available frame or replace the oldest page if memory is full
    if (frame_count < num_frames) 
    {
        int frame_number = frame_count; // Get the next available frame
        frame_queue[queue_rear] = frame_number; // Add the frame to the queue
        queue_rear = (queue_rear + 1) % num_frames; // Move rear pointer
        frame_count++;
        return frame_number; // Return the next available frame
    } 

    int old_page = frame_queue[queue_front];
    queue_front = (queue_front + 1) % num_frames;
    
    // Invalidate the old page and return its frame number for reuse
    for(int i = 0; i < NUM_PAGES; i++)
    {
        // Find the page that is currently using the frame we want to reuse and invalidate it
        if (page_table[i].valid && page_table[i].frame_number == old_page)
        {
            last_replaced_page = i; // Store the last replaced page for logging
            page_table[i].valid = 0; // Invalidate the old page
            for(int j = 0; j < TLB_SIZE; j++)
            {
                if (tlb[j].valid && tlb[j].page_number == i)
                {
                    tlb[j].valid = 0; // Invalidate the TLB entry for the old page
                }
            }
            break;
        }
    }
    
    frame_queue[queue_rear] = old_page; // Add the frame back to the queue
    queue_rear = (queue_rear + 1) % num_frames; // Move rear pointer
    
    return old_page; // Return the frame number of the replaced page
}

int get_frame_lru(void)
{
    // Find the least recently used page and return its frame number for reuse
    int lru_page = -1;
    int lru_time = current_time;
    if(frame_count < num_frames) 
    {
        return frame_count++; // Return the next available frame
    }
    for (int i = 0; i < NUM_PAGES; i++)
    {
        if (page_table[i].valid && page_table[i].access_time < lru_time)
        {
            lru_time = page_table[i].access_time;
            lru_page = i;
        }
    }
    if (lru_page != -1)
    {
        int frame_number = page_table[lru_page].frame_number;
        page_table[lru_page].valid = 0; // Invalidate the least recently used page
        last_replaced_page = lru_page; // Store the last replaced page for logging
        for(int j = 0; j < TLB_SIZE; j++)
        {
            if (tlb[j].valid && tlb[j].page_number == lru_page)
            {
                tlb[j].valid = 0; // Invalidate the TLB entry for the least recently used page
            }
        }
        return frame_number; // Return the frame number of the replaced page
    }
    return 0;
}

int get_frame_random(void)
{
    if(frame_count < num_frames) 
    {
        return frame_count++; // Return the next available frame
    }
    int random_page = rand() % NUM_PAGES; 
    do {
        random_page = rand() % NUM_PAGES; // Select a random page
    } while (!page_table[random_page].valid); // Ensure the selected page is valid

    int frame_number = page_table[random_page].frame_number;
    page_table[random_page].valid = 0; // Invalidate the selected page
    last_replaced_page = random_page; // Store the last replaced page for logging
    for(int j = 0; j < TLB_SIZE; j++)
    {
        if (tlb[j].valid && tlb[j].page_number == random_page)
        {
            tlb[j].valid = 0; // Invalidate the TLB entry for the selected page
        }
    }
    return frame_number; // Return the frame number of the replaced page
}

int translate_address(int logical_address, int *physical_address, FILE *backing_store, int *out_page, int *out_offset, int *out_tlb_hit, int *out_page_fault, int *out_frame)
{
    logical_address = logical_address & 0xFFFF; // Mask to get the rightmost 16 bits

    int page_number = (logical_address >> 8) & 0xFF; // Get the page number (upper 8 bits)
    int offset = logical_address & 0xFF; // Get the offset (lower 8 bits)

    int frame_number;
    int tlb_hit = is_tlb(page_number, &frame_number);

    if(!tlb_hit)
    {
        if(!page_table_check(page_number, &frame_number))
        {
            frame_number = page_fault_handler(page_number, backing_store);
            pg_faults++;
        }
        update_tlb(page_number, frame_number);
    }
    *physical_address = frame_number * PAGE_SIZE + offset; // Calculate physical address


    *out_page = page_number;
    *out_offset = offset;
    *out_tlb_hit = tlb_hit;
    *out_page_fault = !tlb_hit && !page_table_check(page_number, &frame_number);
    *out_frame = frame_number;
    if(verbose_flag)
        printf("    [TLB %s] page %d -> frame %d\n",tlb_hit ? "hit" : "miss", page_number, frame_number);

    return (signed char)physical_memory[*physical_address]; // Return the value at the physical address
}

int page_fault_handler(int page_number, FILE *backing_store)
{
    int frame_number = get_frame();
    load_page_into_memory(page_number, frame_number, backing_store);
    update_page_table(page_number, frame_number);
    if(verbose_flag)
        printf("    [PAGE FAULT] page %d loaded into frame %d\n", page_number, frame_number);
    return frame_number;
}

void load_page_into_memory(int page_number, int frame_number, FILE *backing_store)
{
    long offset = (long)page_number * PAGE_SIZE;

    if(fseek(backing_store, offset, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error seeking in backing store for page %d\n", page_number);
        exit(EXIT_FAILURE);
    }

    char *frame_start = physical_memory + (frame_number * PAGE_SIZE);
    if(fread(frame_start, sizeof(char), PAGE_SIZE, backing_store) != PAGE_SIZE)
    {
        fprintf(stderr, "Error reading page %d from backing store\n", page_number);
        exit(EXIT_FAILURE);
    }
}

void print_stats(void)
{
    printf("Total addresses: %d\n", total_addresses);
    printf("Page faults: %d\n", pg_faults);
    printf("Page fault rate: %.2f%%\n", (double)pg_faults / total_addresses * 100);
    printf("TLB hits: %d\n", tlb_hits);
    printf("TLB misses: %d\n", tlb_misses);
    printf("TLB hit rate: %.2f%%\n", (double)tlb_hits / total_addresses * 100);
}

void write_csv_row(FILE *csv, int logical_address, int page_number, int offset, int tlb_hit, int page_fault, int frame_number, int physical_address, char value_at_address, int replaced_page_frame)
{
    // FILE *csv = fopen("resultsRandomRandomFifo128.csv", "a");
    if (csv == NULL)
    {
        fprintf(stderr, "Error opening csv for appending\n");
        exit(EXIT_FAILURE);
    }
    fprintf(csv, "%d,%d,%d,%s,%s,%d,%d,%d,%d\n", logical_address, page_number, offset, tlb_hit ? "Hit" : "Miss", page_fault ? "Yes" : "No", frame_number, physical_address, value_at_address, replaced_page_frame);
    //fclose(csv);
}

int main(int argc, char *argv[]) 
{    
    int option;
    char tlb_name[16];
    char page_name[16];

    // Define long options
    static struct option long_options[] = 
    {
        {"help",    no_argument,       0, 'h'},
        {"tlb", required_argument,       0, 't'},
        {"page",  required_argument, 0, 'p'},
        {"frames", required_argument, 0, 'f'},
        {"verbose", no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };
    while ((option = getopt_long(argc, argv, "hvo:", long_options, NULL)) != -1)
    {
        switch (option) 
        {
            case 'h':
                printf("Usage: %s <input_file> --t <table_policy> --p <page_policy> --f <frame_amount> [--verbose]\n", argv[0]);
                printf("Example: %s addresses.txt --t lru --p fifo --f 128\n", argv[0]);
                exit(0);
            case 't':
                if(strcmp(optarg, "lru") == 0)
                {
                    update_tlb = update_tlb_lru;
                    strcpy(tlb_name, "lru");
                } else if (strcmp(optarg, "random") == 0)
                {
                    update_tlb = update_tlb_random;
                    strcpy(tlb_name, "random");
                } else
                {
                    fprintf(stderr, "Invalid TLB policy: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'p':
                if(strcmp(optarg, "fifo") == 0)
                {
                    update_page_table = update_page_table_fifo;
                    get_frame = get_frame_fifo;
                    strcpy(page_name, "fifo");
                } else if (strcmp(optarg, "lru") == 0)
                {
                    update_page_table = update_page_table_lru;
                    get_frame = get_frame_lru;
                    strcpy(page_name, "lru");
                } else if (strcmp(optarg, "random") == 0)
                {
                    update_page_table = update_page_table_random;
                    get_frame = get_frame_random;
                    strcpy(page_name, "random");
                } else
                {
                    fprintf(stderr, "Invalid page replacement policy: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'f':
                num_frames = atoi(optarg);
                if (num_frames <= 0 || num_frames > 256)
                {
                    fprintf(stderr, "Invalid number of frames: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'v':
                verbose_flag = 1;
                break;
            default:
                abort();
        }
    }

    if(optind >= argc)
    {
        fprintf(stderr, "Error: no input file provided, --h for help\n");
        fprintf(stderr, "Usage: %s <input_file> --t <table_policy> --p <page_policy> --f <frame_amount> [--verbose]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if(!update_tlb)
    {
        fprintf(stderr, "Error: no TLB policy specified. Use -t lru or -t random\n");
        exit(EXIT_FAILURE);
    }
    if(!get_frame || !update_page_table)
    {
        fprintf(stderr, "Error: no page replacement policy specified. Use -p fifo, -p lru, or -p random\n");
        exit(EXIT_FAILURE);
    }

    physical_memory = malloc(num_frames * PAGE_SIZE);
    frame_queue = malloc(num_frames * sizeof(int));

    init_tlb();
    init_page_table();

    FILE *address_file = fopen(argv[optind], "r");
    if(!address_file)
    {
        fprintf(stderr, "Error opening address file: %s\n", argv[optind]);
        exit(EXIT_FAILURE);
    }
    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");
    if(!backing_store)
    {
        fprintf(stderr, "Error opening backing store file: BACKING_STORE.bin\n");
        exit(EXIT_FAILURE);
    }
    char csv_name[256];
    snprintf(csv_name, sizeof(csv_name), "results_%s_%s_%d.csv", tlb_name, page_name, num_frames);
    FILE *csv = fopen(csv_name, "w");
    if(!csv)
    {
        fprintf(stderr, "Error opening results.csv for writing\n");
        exit(EXIT_FAILURE);
    }
    
    fprintf(csv, "Logical Address,Page Number,Offset,TLB Hit,Page Fault,Frame Number,Physical Address,Value at Address,Replaced Page/Frame\n");

    int logical_addr;
    while (fscanf(address_file, "%d", &logical_addr) == 1)
    {
        last_replaced_page = -1;
        int physical_addr, page_number, offset, tlb_hit, page_fault, frame_number;
        total_addresses++;
        int value = translate_address(logical_addr, &physical_addr, backing_store, &page_number, &offset, &tlb_hit, &page_fault, &frame_number);
        printf("Virtual Address: %d", logical_addr);
        printf(", Physical Address: %d, Value: %d\n", physical_addr, value);
        write_csv_row(csv, logical_addr, page_number, offset, tlb_hit, page_fault, frame_number, physical_addr, value, last_replaced_page);
    }

    print_stats();

    
    fclose(address_file);
    fclose(backing_store);
    fclose(csv);
    free(physical_memory);
    free(frame_queue);
    return EXIT_SUCCESS;
}
