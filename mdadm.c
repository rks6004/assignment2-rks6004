#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"
#include <tester.h>

#define TRUE 1
#define FALSE 0

int mdadm_mounted = FALSE;

int mdadm_mount(void) {
  // Complete your code here
  //printf("\ncurrent JBOD status before mounting: %d\n", jbod_error);
  if (mdadm_mounted) {
    //only need to fill out "op" field since all other fields ignored for mounting
    //printf("current JBOD status after array already mounted: %d\n", jbod_error);
    return -1;
  }
  int jbod_op_return =  jbod_operation((uint32_t)(JBOD_MOUNT << RESERVED_WIDTH), NULL);
  //printf("jbod_operation return value during jbod_mount func: %d\n", jbod_op_return);
  if (jbod_op_return == -1) {
    // printf("current JBOD status after failed mounting: %d\n", jbod_error);
    return -1;  
  }
  // printf("current JBOD status after successful mounting: %d\n", jbod_error);
  mdadm_mounted = TRUE;
  return 1;
}

int mdadm_unmount(void) {
  //Complete your code here
  // printf("\ncurrent JBOD status before unmounting: %d\n", jbod_error);
  if (!mdadm_mounted) {
    //only need to fill out "op" field since all other fields ignored for mounting
    // printf("current JBOD status after array already unmounted: %d\n", jbod_error);
    return -1;
  }
  int jbod_op_return =  jbod_operation((uint32_t)(JBOD_UNMOUNT << RESERVED_WIDTH), NULL);
  // printf("jbod_operation return value during mdam_unmount func: %d\n", jbod_op_return);
  if (jbod_op_return == -1) {
    // printf("current JBOD status after failed unmounting: %d\n", jbod_error);
    return -1;  
  }
  // printf("current JBOD status after successful unmounting: %d\n", jbod_error);
  mdadm_mounted = FALSE;
  return 1;
}

//disk locator to use in opcode population
int disk_locator(uint32_t addr) {
  int disk_calculated = (addr)/JBOD_DISK_SIZE;
  return disk_calculated;
}

//block locator to use in opcode population
int block_locator(uint32_t addr) {  
  //get BYTE-BASED address of addr
  int disk_location = disk_locator(addr);
  uint32_t byte_based_addr = addr - (disk_location * JBOD_DISK_SIZE);
  return byte_based_addr / JBOD_BLOCK_SIZE; 
}

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)  {
  //Complete your code here
  if (start_addr + read_len >= JBOD_TOTAL_ARRAY_SIZE) {
    return OUT_OF_BOUNDS;
  }
  if (read_len > 1024) {
    return EXCESS_BYTES;
  }
  if (read_len < 0 || (read_len > 0 && read_buf == NULL)) {
    return BAD_ARGS;
  }
  if (read_len == 0) {
    return 0;
  }
  if (!mdadm_mounted) {
    return ARRAY_UNMOUNTED;
  }

  //checking first case: reading within same block
  //comparing by seeing if start_addr's BLOCK-REFERENCED ADDRESS 
  //+ read_len bytes is less than max block addr.
  //memcpy works direct here- can just read read_len bytes from start_addr into read_buf loc.
  if ((start_addr % JBOD_BLOCK_SIZE) + read_len < JBOD_BLOCK_SIZE) {
    printf("\nwe are NOT reading beyond a single block.\n");
    //opcode for seeking disk, referencing disk_locator helpful func
    uint32_t seek_disk_code = (disk_locator(start_addr) 
      << (sizeof(seek_disk_code)*8 - DISK_ID_WIDTH)) 
      + (JBOD_SEEK_TO_DISK << RESERVED_WIDTH); 
    if (jbod_operation(seek_disk_code, NULL) == -1) {return -4;}
    //opcode for seeking disk, referencing block_locator helpful func
    uint32_t seek_block_code = (block_locator(start_addr) 
      << (sizeof(seek_block_code)*8 - DISK_ID_WIDTH - BLOCK_ID_WIDTH)) 
      + (JBOD_SEEK_TO_DISK << RESERVED_WIDTH); 
    if (jbod_operation(seek_block_code, NULL) == -4) {return -4;}
    uint32_t read_code = JBOD_READ_BLOCK << RESERVED_WIDTH;
    //would this work?
    //type asserts 1 byte, yet size declaration states 256 bytes
    uint8_t temp_buffer[256];
    if (jbod_operation(read_code, temp_buffer) == -4) {return -4;}
    //copying memory from where start_addr lies IN BLOCK
    //for read_len bits
    memcpy(read_buf, temp_buffer + (start_addr % JBOD_BLOCK_SIZE), read_len);
  }
  //reading beyond a single block
  else {
    printf("\nwe are reading beyond a single block.");
    uint32_t end_addr = start_addr + read_len;
    printf("\nstart address: %u, end address calculated as: %u\n", start_addr, end_addr);
    int start_disk = disk_locator(start_addr);
    int end_disk = disk_locator(end_addr);
    printf("starting at disk: %d and ending at disk: %d\n", start_disk, end_disk);
    int bytes_read = 0;
    uint32_t addr_tracker = start_addr;
    for (int d = start_disk; d <= end_disk; d++) {
      //only filling out necessary aspects of opcode
      uint32_t seek_disk_code = (d << (sizeof(seek_disk_code)*8 - DISK_ID_WIDTH)) 
        + (JBOD_SEEK_TO_DISK << RESERVED_WIDTH); 
      jbod_operation(seek_disk_code, NULL);
      //we know that this read will stretch past the same block
      int start_block = block_locator(addr_tracker);
      int end_block;
      if (d == end_disk) {
         end_block = block_locator(end_addr);
      }
      else {
        end_block = JBOD_NUM_BLOCKS_PER_DISK - 1;
      }
      printf("\nFor disk: %d, reading from start block: %d to end block: %d\n", d, start_block, end_block);
      uint32_t seek_block_code = 
          (start_block << (sizeof(seek_disk_code)*8 - DISK_ID_WIDTH))
          + (JBOD_SEEK_TO_BLOCK << RESERVED_WIDTH);
      jbod_operation(seek_block_code, NULL);
      //increment will be used for TRACKING purposes- JBOD_READ increments block value itself
      //will still have to reference b in order to jump "back" to new disk when necessary
      //since JBOD_READ will likely throw errors if reading beyond disk
      for (int b = start_block; b <= end_block; b++) {
        uint32_t read_code = JBOD_READ_BLOCK << RESERVED_WIDTH;
        uint8_t temp_buffer[256];
        jbod_operation(read_code, temp_buffer);
        //since within block now, now have to know where current address lies IN BLOCK (which byte)
        uint8_t addr_tracker_byte = addr_tracker % JBOD_BLOCK_SIZE;
        //copying memory from where addr lies IN BLOCK
        //for read_len bits
        uint8_t temp_bytes;
        //if within last block, read number of EXCESS bytes to read
        if (b == end_block) {
          temp_bytes = (end_addr % JBOD_BLOCK_SIZE) - addr_tracker_byte;
        }
        //otherwise, read till end of block
        else {
          printf("ELse\n");
          temp_bytes = JBOD_BLOCK_SIZE - addr_tracker_byte;
          printf("Addr tracker statement: %u", addr_tracker_byte);
        }
        printf("\nFor block %d, reading %u bytes\n", b, temp_bytes);
        //operation sees temp_bytes being copied from addr_tracker's BLOCK BASED value
        //to read_buf at its "latest" location
        memcpy(read_buf + bytes_read, temp_buffer + addr_tracker_byte, temp_bytes);
        printf("Addr_tracker was at: %u\n", addr_tracker_byte);
        printf("Total bytes read before block operation: %d\n", bytes_read);
        printf("Bytes read during block operation: %d\n", temp_bytes);
        addr_tracker += temp_bytes;
        bytes_read += temp_bytes;
        printf("Total bytes read after block operation: %d\n", bytes_read);
      }
    }
  }
  return read_len;
}

