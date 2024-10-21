#ifndef JBOD_H_
#define JBOD_H_
#include <stdint.h>

// Fill the below values with right values
//byte-addressed
#define JBOD_NUM_DISKS            16 //from README
#define JBOD_BLOCK_SIZE           256 //from README
#define JBOD_TOTAL_ARRAY_SIZE     1048576 //from README
#define JBOD_NUM_BLOCKS_PER_DISK  (int)(((int)(JBOD_TOTAL_ARRAY_SIZE/JBOD_BLOCK_SIZE))/JBOD_NUM_DISKS)

#define JBOD_DISK_SIZE            JBOD_BLOCK_SIZE * JBOD_NUM_BLOCKS_PER_DISK


//bit widths for reference (all from README)
#define DISK_ID_WIDTH 4
#define BLOCK_ID_WIDTH 8
#define CMD_ID_WIDTH 8
#define RESERVED_WIDTH 12


typedef enum {
  JBOD_MOUNT,
  JBOD_UNMOUNT,
  JBOD_SEEK_TO_DISK,
  JBOD_SEEK_TO_BLOCK,
  JBOD_READ_BLOCK,
  JBOD_WRITE_BLOCK,
  JBOD_SIGN_BLOCK,
  JBOD_NUM_CMDS,
} jbod_cmd_t;

typedef enum {
  JBOD_NO_ERROR,
  JBOD_UNMOUNTED,
  JBOD_ALREADY_MOUNTED,
  JBOD_ALREADY_UNMOUNTED,
  JBOD_CACHELOAD_FAIL,
  JBOD_CACHEWRITE_FAIL,
  JBOD_BAD_CMD,
  JBOD_BAD_DISK_NUM,
  JBOD_BAD_BLOCK_NUM,
  JBOD_BAD_READ,
  JBOD_BAD_WRITE,
  JBOD_NUM_ERRNOS,
} jbod_error_t;

int jbod_operation(uint32_t op, uint8_t *block);

extern jbod_error_t jbod_error;
const char *jbod_error_string(int eno);

#endif
