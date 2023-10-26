#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <unistd.h>

#include <pthread.h>
#include <linux/sched.h>

#include <fftw3.h>
#include <math.h>

#define ASSERT(c) do { if (!(c)) {                                                \
  printf("Assertion failed: %s\n", #c ); fflush(stdout); (*(volatile int*)0 = 0); \
} } while(0)

#define RANDOM_NUMBER_GENERATOR_SEED 64

#define TASKS_AMOUNT 3

#define GB(x) ((size_t)(x) * 1024 * 1024 * 1024)

#define internal static
#define global static

//- rhjr: mathematics

typedef union Vec3I64 Vec3I64;
union Vec3I64
{
  struct
  {
    int64_t x, y, z;
  };

  int64_t elements[3];
};

typedef union Vec9I64 Vec9I64;
union Vec9I64
{
  int64_t elements[9];
};

typedef union Mat3x3I64 Mat3x3I64;
union Mat3x3I64 
{
  int64_t elements[3][3];
