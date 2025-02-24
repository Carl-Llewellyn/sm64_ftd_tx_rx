#include "include/WinTypes.h"
#include "include/ftd2xx.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef enum {
  DATATYPE_TEXT = 0x01,
  DATATYPE_RAWBINARY = 0x02,
  DATATYPE_HEADER = 0x03,
  DATATYPE_SCREENSHOT = 0x04,
  DATATYPE_HEARTBEAT = 0x05,
  DATATYPE_RDBPACKET = 0x06
} USBDataType;

#define SHM_NAME "/sm64x"
#define SIZE 8096

char *shm;

int setUpSharedMem() {

  int shm_fd;

  // Create (or open) a shared memory object.
  shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }

  // Set the size of the shared memory object.
  if (ftruncate(shm_fd, SIZE) == -1) {
    perror("ftruncate");
    exit(EXIT_FAILURE);
  }

  // Map the shared memory object into the process's address space.
  shm = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (shm == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  memset(shm, 0, SIZE);

  // strcpy(shm, "start");

  // Cleanup: Unmap and unlink shared memory.
  // munmap(ptr, SIZE);
  // shm_unlink(SHM_NAME);

  return 0;
}

uint32_t swap_endian(uint32_t val) {
  return ((val << 24)) | ((val << 8) & 0x00ff0000) | ((val >> 8) & 0x0000ff00) |
         ((val >> 24));
}

void freeMods() {
  if (system("sudo rmmod ftdi_sio") != 0) {
    perror("Error removing ftdi_sio module\n");
    // return 1;
  }

  if (system("sudo rmmod usbserial") != 0) {
    perror("Error removing usbserial module\n");
    // return 1;
  }
}

int getDevDetails(FT_DEVICE_LIST_INFO_NODE *ftdidevs) {
  int devNum = 0;
  int status = 0;

  if (FT_CreateDeviceInfoList((LPDWORD)&devNum) != FT_OK) {
    printf("Error creating device info list.\n");
    return 1;
  }

  printf("Devices: %d\n", devNum);

  if (FT_GetDeviceInfoList(ftdidevs, (LPDWORD)&devNum) != FT_OK) {
    printf("Error getting device info list.\n");
    free(ftdidevs); // Ensure we free allocated memory if there's an error
    return 1;
  }

  if (ftdidevs[0].Flags & FT_FLAGS_OPENED) {
    printf("Device is already open. Exiting...\n");
    free(ftdidevs); // Free allocated memory before exiting
    return 1;
  }

  printf("Status: %d, ID: %d, Desc: %s\n", status, ftdidevs[0].ID,
         ftdidevs[0].Description);
}

int openDevice(FT_HANDLE *handle) {
  printf("Opening device...\n");

  // Pass the address of handle
  if (FT_Open(0, handle) != FT_OK) {
    printf("Error opening device.\n");
    return 1;
  }

  printf("Device opened.\n");
  return 0;
}

int readData(FT_HANDLE *handle) {
  int bytesInQueue = 0;
  char buffer[(16 * 4)];
  uint32_t size = sizeof(buffer);
  uint32_t bytesRead = 0, totalRead = 0;

  // Check queue status
  if (FT_GetQueueStatus(*handle, (LPDWORD)&bytesInQueue) != FT_OK) {
    printf("Error getting queue status\n");
    return 1;
  }

  if (bytesInQueue == 0) {
    return 1;
  }

  FT_STATUS status = FT_Read(*handle, (LPVOID)(buffer + totalRead), bytesInQueue,
                             (LPDWORD)&bytesRead);


  if (status != FT_OK) {
    printf("FT_READ status err");
    return 1;
  }

  if (bytesRead < bytesInQueue){
    printf("Read bytes < queue bytes");
    return 1;
  }

  if (bytesRead > 0) {
    memcpy(shm, buffer, bytesRead);
  }

  return 0;
}



int sendData(FT_HANDLE *handle, uint8_t data[12]) {
  uint32_t headerSize = 0;
  uint32_t bytesWritten = 0;
  uint32_t dataSize = sizeof(*data);

  uint32_t bytes;
  uint8_t header[12];

  // Prepare command header
  header[0] = (uint8_t)'C';
  header[1] = (uint8_t)'M';
  header[2] = (uint8_t)'D';
  header[3] = 'U';
  *(uint32_t *)(&header[4]) = swap_endian(DATATYPE_TEXT);
  *(uint32_t *)(&header[8]) = swap_endian(4);

  headerSize = sizeof(header);

  // header
  if (FT_Write(*handle, header, sizeof(header), (LPDWORD)&bytesWritten) !=
      FT_OK) {
    printf("Error writing header data.\n");
    return 1;
  }

  if (bytesWritten != headerSize) {
    printf("Error: header byte count mismatch. Expected %u, wrote %u bytes\n",
           headerSize, bytesWritten);
    return 1;
  }

  // data
  if (FT_Write(*handle, data, dataSize, (LPDWORD)&bytesWritten) != FT_OK) {
    printf("Error writing write data payload.\n");
    return 1;
  }

  if (bytesWritten != dataSize) {
    printf("Error: header data payload count mismatch. Expected %u, wrote %u "
           "bytes\n",
           dataSize, bytesWritten);
    return 1;
  }

  return 0;
}

int main(void) {
  int devNum = 0;
  int status = 0;
  FT_HANDLE handle;
  FT_DEVICE_LIST_INFO_NODE *ftdidevs = (FT_DEVICE_LIST_INFO_NODE *)malloc(
      sizeof(FT_DEVICE_LIST_INFO_NODE) * devNum);

  freeMods();
  getDevDetails(ftdidevs);
  if (openDevice(&handle)) {
    free(ftdidevs);
    return 1;
  }

  uint8_t data[12];
  data[0] = (uint8_t)'T';
  data[1] = (uint8_t)'E';
  data[2] = (uint8_t)'S';
  data[3] = 'T';
  *(uint32_t *)(&data[4]) = swap_endian(DATATYPE_TEXT);
  *(uint32_t *)(&data[8]) = swap_endian(4);

  setUpSharedMem();
  
  while (1) {
    readData(&handle);
    // sendData(&handle, data);
    usleep(10000);
  }

  // Free memory and cleanup
  free(ftdidevs);

  // Close the device when done
  FT_Close(handle);
  printf("Device closed.\n");

  return 0;
}