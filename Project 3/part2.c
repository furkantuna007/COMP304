/**
 * virtmem.c
 * Comp304 - Project3 - Part 2
 * Yiğithan Şanlık 64117
 * Furkan Tuna 69730
 */
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define TLB_SIZE 16
#define PAGES 1024
#define PAGE_MASK 1023  // Mask

#define PAGE_SIZE 1024
#define OFFSET_BITS 10
#define OFFSET_MASK 1023 // Mask

#define MEMORY_SIZE 256 * PAGE_SIZE

// Max number of characters per line of input file to read.
#define BUFFER_SIZE 10

struct tlbentry {
  unsigned char logical;
  unsigned char physical;
  unsigned char reference_bit;  // For Second Chance replacement
  unsigned char modified_bit;   // For Second Chance replacement
  int timestamp;                // For LRU replacement
};

// TLB is kept track of as a circular array, with the oldest element being overwritten once the TLB is full.
struct tlbentry tlb[TLB_SIZE];
// Number of inserts into TLB that have been completed. Use as tlbindex % TLB_SIZE for the index of the next TLB line to use.
int tlbindex = 0;

// Page table: pagetable[logical_page] is the physical page number for logical page. Value is -1 if that logical page isn't yet in the table.
int pagetable[PAGES];

signed char main_memory[MEMORY_SIZE];

// Pointer to memory mapped backing file
signed char *backing;

// Free page frames bitmap: 0 means a page frame is free, 1 means it is occupied
int free_page_frames[MEMORY_SIZE / PAGE_SIZE];

// Global timestamp or counter for LRU replacement policy
int timestamp = 0;

int max(int a, int b)
{
  if (a > b)
    return a;
  return b;
}

// Returns the physical address from TLB or -1 if not present.
int search_tlb(unsigned char logical_page) {
  int i;
  for (i = max(0, tlbindex - TLB_SIZE); i < tlbindex; i++) {
    if (tlb[i % TLB_SIZE].logical == logical_page) {
      tlb[i % TLB_SIZE].reference_bit = 1;  // Set reference bit
      return tlb[i % TLB_SIZE].physical;
    }
  }
  return -1;
}

// Adds the specified mapping to the TLB,
// replacing the oldest mapping based on the selected page-replacement policy.
void add_to_tlb(unsigned char logical, unsigned char physical, int page_replacement_policy) {
  if (page_replacement_policy == 0) {
    //Second Chance
    while (tlb[tlbindex % TLB_SIZE].reference_bit) {
      tlb[tlbindex % TLB_SIZE].reference_bit = 0;  // Set reference bit to 0 to clear.
      tlbindex++;
    }
  } else if (page_replacement_policy == 1) {
    // In here we implemented LRU.
    int min_timestamp = tlb[0].timestamp;
    int min_index = 0;
    int i;
    for (i = 1; i < TLB_SIZE; i++) {
      if (tlb[i].timestamp < min_timestamp) {
        min_timestamp = tlb[i].timestamp;
        min_index = i;
      }
    }
    tlbindex = min_index + 1;
  }

  // We Update the TLB entries.
  tlb[tlbindex % TLB_SIZE].logical = logical;
  tlb[tlbindex % TLB_SIZE].physical = physical;
  tlb[tlbindex % TLB_SIZE].reference_bit = 0;  // Set reference bit to 0
  tlb[tlbindex % TLB_SIZE].modified_bit = 0;   // Set modified bit to 0
  tlb[tlbindex % TLB_SIZE].timestamp = timestamp;  // Update timestamp for LRU policy
  tlbindex++;
}

//In here we update the timestamp for all TLB entries
void update_timestamp() {
  int i;
  for (i = 0; i < TLB_SIZE; i++) {
    tlb[i].timestamp++;
  }
}

// In here we find a free page frame from the physical memory with this function.
int find_free_page_frame() {
  int i;
  for (i = 0; i < MEMORY_SIZE / PAGE_SIZE; i++) {
    if (free_page_frames[i] == 0) {
      free_page_frames[i] = 1;  //set to page frame = occupied
      return i;
    }
  }
  return -1;
}

int main(int argc, const char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage ./virtmem backingstore input -p [0: Second Chance, 1: LRU]\n");
    exit(1);
  }

  const char *backing_filename = argv[1];
  int backing_fd = open(backing_filename, O_RDONLY);
  backing = mmap(0, MEMORY_SIZE, PROT_READ, MAP_PRIVATE, backing_fd, 0);

  const char *input_filename = argv[2];
  FILE *input_fp = fopen(input_filename, "r");

  //In here we fill page table entries.
  int i;
  for (i = 0; i < PAGES; i++) {
    pagetable[i] = -1;
  }

  // In here we initialized the reference_bit for each TLB entry.
  for (i = 0; i < TLB_SIZE; i++) {
    tlb[i].reference_bit = 0;
    tlb[i].modified_bit = 0;
    tlb[i].timestamp = -1;
  }

  char buffer[BUFFER_SIZE];


  int total_addresses = 0;
  int tlb_hits = 0;
  int page_faults = 0;


  unsigned char free_page = 0;

  //Page-replacement policy: 0 for Second Chance, 1 for LRU
  int page_replacement_policy = atoi(argv[3]);

  while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
    total_addresses++;
    int logical_address = atoi(buffer);

    //Calculate the page offset + logical page
    int offset = logical_address & OFFSET_MASK;
    int logical_page = (logical_address >> OFFSET_BITS) & PAGE_MASK;

    int physical_page = search_tlb(logical_page);
    //TLB hit.
    if (physical_page != -1) {
      tlb_hits++;
      //TLB miss.
    } else {
      physical_page = pagetable[logical_page];

      //Page fault
      if (physical_page == -1) {
        page_faults++;
        // Find a free page frame in physical memory
        int free_frame = find_free_page_frame();


        if (free_frame == -1) {
          // In here we implemented, Second Chance page replacement policy.
          if (page_replacement_policy == 0) {
            while (1) {
              int page_to_replace = tlb[tlbindex % TLB_SIZE].logical;
              int frame_to_replace = tlb[tlbindex % TLB_SIZE].physical;
              int reference_bit = tlb[tlbindex % TLB_SIZE].reference_bit;
              int modified_bit = tlb[tlbindex % TLB_SIZE].modified_bit;


              if (reference_bit == 0) {
                free_frame = frame_to_replace;
                tlbindex++;
                break;
              }

              //ref bit = 0
              tlb[tlbindex % TLB_SIZE].reference_bit = 0;
              tlbindex++;
            }
          }
          //In here we implemented LRU page replacement policy
          else if (page_replacement_policy == 1) {
            int min_timestamp = tlb[0].timestamp;
            int min_index = 0;
            int i;
            for (i = 1; i < TLB_SIZE; i++) {
              if (tlb[i].timestamp < min_timestamp) {
                min_timestamp = tlb[i].timestamp;
                min_index = i;
              }
            }
            free_frame = tlb[min_index].physical;
            //Start from the next index after the LRU entry.
            tlbindex = min_index + 1;
          }
        }


        memcpy(main_memory + (free_frame * PAGE_SIZE), backing + (logical_page * PAGE_SIZE), PAGE_SIZE);
        physical_page = free_frame;

        //Update the page table
        pagetable[logical_page] = physical_page;

        //Add the new mapping to the TLB
        add_to_tlb(logical_page, physical_page, page_replacement_policy);
      } else {

        add_to_tlb(logical_page, physical_page, page_replacement_policy);
      }
    }

    //Calc padress
    int physical_address = (physical_page << OFFSET_BITS) | offset;

    //value at the physical address
    signed char value = main_memory[physical_page * PAGE_SIZE + offset];

    printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, physical_address, value);

    //Update timestamp
    update_timestamp();
    timestamp++;
  }
  printf("Number of Translated Addresses = %d\n", total_addresses);

  //In here we can compute hit rate and fault rate stats.
  float tlb_hit_rate = (float)tlb_hits / total_addresses;
  float page_fault_rate = (float)page_faults / total_addresses;
  
  printf("TLB Hits: %d TLB Hit Rate: %.3f\n", tlb_hits, tlb_hit_rate);
  printf("Page Faults: %d Page Fault Rate: %.3f\n", page_faults, page_fault_rate);
  


  munmap(backing, MEMORY_SIZE);
  fclose(input_fp);
  close(backing_fd);

  return 0;
}
