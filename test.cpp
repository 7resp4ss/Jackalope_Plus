/*
Copyright 2020 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>

// shared memory stuff

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#define MAX_SAMPLE_SIZE 1000000
#define SHM_SIZE (4 + MAX_SAMPLE_SIZE)
unsigned char *shm_data;

bool use_shared_memory;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)

int setup_shmem(const char* name) {
  HANDLE map_file;

  map_file = OpenFileMapping(
    FILE_MAP_ALL_ACCESS,   // read/write access
    FALSE,                 // do not inherit the name
    name);            // name of mapping object

  if (map_file == NULL) {
    printf("Error accessing shared memory\n");
    return 0;
  }

  shm_data = (unsigned char*)MapViewOfFile(map_file, // handle to map object
    FILE_MAP_ALL_ACCESS,  // read/write permission
    0,
    0,
    SHM_SIZE);

  if (shm_data == NULL) {
    printf("Error accessing shared memory\n");
    return 0;
  }

  return 1;
}

#else

int setup_shmem(const char *name)
{
#ifdef __ANDROID__
  printf("Shared memory not supported on Android\n");
  return 0;
#else
  int fd;

  // get shared memory file descriptor (NOT a file)
  fd = shm_open(name, O_RDONLY, S_IRUSR | S_IWUSR);
  if (fd == -1)
  {
    printf("Error in shm_open\n");
    return 0;
  }

  // map shared memory to process address space
  shm_data = (unsigned char *)mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
  if (shm_data == MAP_FAILED)
  {
    printf("Error in mmap\n");
    return 0;
  }

  return 1;
#endif
}

#endif

// used to force a crash
char *crash = NULL;

// ensure we can find the target

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#define FUZZ_TARGET_MODIFIERS __declspec(dllexport)
#else
#define FUZZ_TARGET_MODIFIERS __attribute__ ((noinline))
#endif

// actual target function

void FUZZ_TARGET_MODIFIERS fuzz(char *name) {
  char *sample_bytes = NULL;
  uint32_t sample_size = 0;
  
  // read the sample either from file or
  // shared memory
  if(use_shared_memory) {
    sample_size = *(uint32_t *)(shm_data);
    if(sample_size > MAX_SAMPLE_SIZE) sample_size = MAX_SAMPLE_SIZE;
    sample_bytes = (char *)malloc(sample_size);
    memcpy(sample_bytes, shm_data + sizeof(uint32_t), sample_size);
  } else {
    FILE *fp = fopen(name, "rb");
    if(!fp) {
      printf("Error opening %s\n", name);
      return;
    }
    fseek(fp, 0, SEEK_END);
    sample_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    sample_bytes = (char *)malloc(sample_size);
    fread(sample_bytes, 1, sample_size, fp);
    fclose(fp);
  }
  
  int tmp = 0;

  if (sample_size < 4) {
      if (sample_size == 2) {
          tmp += 12;  // 表示 "Size is exactly 2 bytes"
      } else if (sample_size == 3) {
          tmp += 13;  // 表示 "Size is exactly 3 bytes"
      } else if (sample_size == 1) {
          tmp += 14;  // 表示 "Size is exactly 1 byte"
      } else {
          tmp += 15;  // 表示 "Size < 4 and not 1, 2, or 3"
      }
  } else {
      // sample_size >= 4
      if (sample_bytes[0] == 'a') {
          if (sample_bytes[1] == 'b') {
              if (sample_bytes[2] == 'c') {
                  if (sample_bytes[3] == 'd') {
                      // 匹配 "abcd"
                      if (sample_size >= 8) {
                          if (sample_bytes[4] == 'e') {
                              if (sample_bytes[5] == 'f') {
                                  if (sample_bytes[6] == 'g') {
                                      if (sample_bytes[7] == 'h') {
                                          // 匹配 "efgh"
                                          if (sample_size >= 12) {
                                              if (sample_bytes[8] == 'i') {
                                                  if (sample_bytes[9] == 'j') {
                                                      if (sample_bytes[10] == 'k') {
                                                          if (sample_bytes[11] == 'l') {
                                                              // 匹配 "ijkl"
                                                              if (sample_size >= 16) {
                                                                  if (sample_bytes[12] == 'm') {
                                                                      if (sample_bytes[13] == 'n') {
                                                                          if (sample_bytes[14] == 'o') {
                                                                              if (sample_bytes[15] == 'p') {
                                                                                  // 匹配 "mnop"
                                                                                  crash[0] = 1;
                                                                              } else {
                                                                                  tmp += 1;  // 表示 "Matches 'abcd efgh ijkl' but not 'mnop'"
                                                                              }
                                                                          } else {
                                                                              tmp += 1;
                                                                          }
                                                                      } else {
                                                                          tmp += 1;
                                                                      }
                                                                  } else {
                                                                      tmp += 1;
                                                                  }
                                                              } else {
                                                                  tmp += 2;  // 表示 "Matches 'abcd efgh ijkl' but < 16 bytes"
                                                              }
                                                          } else {
                                                              tmp += 3;  // 表示 "Matches 'abcd efgh' but not 'ijkl'"
                                                          }
                                                      } else {
                                                          tmp += 3;
                                                      }
                                                  } else {
                                                      tmp += 3;
                                                  }
                                              } else {
                                                  tmp += 3;
                                              }
                                          } else {
                                              tmp += 4;  // 表示 "Matches 'abcd efgh' but < 12 bytes"
                                          }
                                      } else {
                                          tmp += 5;  // 表示 "Starts with 'abcd' but next four != 'efgh'"
                                      }
                                  } else {
                                      tmp += 5;
                                  }
                              } else {
                                  tmp += 5;
                              }
                          } else {
                              tmp += 5;
                          }
                      } else {
                          tmp += 6;  // 表示 "Starts with 'abcd' but < 8 bytes"
                      }
                  } else {
                      tmp += 11;  // 表示 "Does not start with 'abcd', '1234', 'ABCD', or 'xyzw'"
                  }
              } else {
                  tmp += 11;
              }
          } else {
              tmp += 11;
          }
      } else if (sample_bytes[0] == '1') {
          if (sample_bytes[1] == '2') {
              if (sample_bytes[2] == '3') {
                  if (sample_bytes[3] == '4') {
                      // 匹配 "1234"
                      if (sample_size >= 6) {
                          tmp += 7;  // 表示 "Starts with '1234' and >= 6 bytes"
                      } else {
                          tmp += 8;  // 表示 "Starts with '1234' but < 6 bytes"
                      }
                  } else {
                      tmp += 11;
                  }
              } else {
                  tmp += 11;
              }
          } else {
              tmp += 11;
          }
      } else if (sample_bytes[0] == 'A') {
          if (sample_bytes[1] == 'B') {
              if (sample_bytes[2] == 'C') {
                  if (sample_bytes[3] == 'D') {
                      // 匹配 "ABCD"
                      tmp += 9;  // 表示 "Starts with 'ABCD'"
                  } else {
                      tmp += 11;
                  }
              } else {
                  tmp += 11;
              }
          } else {
              tmp += 11;
          }
      } else if (sample_bytes[0] == 'x') {
          if (sample_bytes[1] == 'y') {
              if (sample_bytes[2] == 'z') {
                  if (sample_bytes[3] == 'w') {
                      // 匹配 "xyzw"
                      tmp += 10;  // 表示 "Starts with 'xyzw'"
                  } else {
                      tmp += 11;
                  }
              } else {
                  tmp += 11;
              }
          } else {
              tmp += 11;
          }
      } else {
          tmp += 11;  // 表示 "Does not start with 'abcd', '1234', 'ABCD', or 'xyzw'"
      }
  }
  if(sample_bytes) free(sample_bytes);
}

int main(int argc, char **argv)
{
  if(argc != 3) {
    printf("Usage: %s <-f|-m> <file or shared memory name>\n", argv[0]);
    return 0;
  }
  
  if(!strcmp(argv[1], "-m")) {
    use_shared_memory = true;
  } else if(!strcmp(argv[1], "-f")) {
    use_shared_memory = false;
  } else {
    printf("Usage: %s <-f|-m> <file or shared memory name>\n", argv[0]);
    return 0;
  }

  // map shared memory here as we don't want to do it
  // for every operation
  if(use_shared_memory) {
    if(!setup_shmem(argv[2])) {
      printf("Error mapping shared memory\n");
    }
  }

  fuzz(argv[2]);
  
  return 0;
}
