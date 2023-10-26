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
  Vec3I64 columns[3];
};

//- rhjr: arena allocator

typedef struct Arena Arena;
struct Arena
{
  uint64_t *memory;
  uint64_t commit_pos;
  uint64_t size;
};

internal void 
arena_initialize(Arena *arena, void* backing_buffer, uint64_t size)
{
  arena->memory = backing_buffer;
  arena->commit_pos = 0;
  arena->size = size;
}

internal void *
arena_alloc(Arena *arena, uint64_t size)
{
	if (arena->commit_pos + size <= arena->size) {
    void* ptr = (void*)((uintptr_t) arena->memory + (uintptr_t) arena->commit_pos);
		arena->commit_pos += size;

		memset(ptr, 0, size);
		return ptr;
	}

	return NULL;
}

internal void
arena_free(Arena *arena)
{
  arena->commit_pos = 0;
}

//- rhjr: multi-threading
typedef struct sched_param ThreadConfig;

typedef union ThreadParameters ThreadParameters;
union ThreadParameters
{
  struct {
    Mat3x3I64 *matrix_sample_data;
    uint64_t matrix_sample_data_size;
  };
  struct {
    Vec9I64 *vector_sample_data;
    uint64_t vector_sample_data_size;
  }; 
  struct {
    fftw_complex *fft_sample_data;
    uint64_t fft_sample_data_size;
  }; 
};

typedef enum ThreadPolicy ThreadPolicy;
enum ThreadPolicy 
{
  THREAD_POLICY_OTHER    = 0x0, // SCHED_OTHER
  THREAD_POLICY_FIFO     = 0x1, // SCHED_FIFO
  THREAD_POLICY_RR       = 0x2, // SCHED_RR
  THREAD_POLICY_BATCH    = 0x3, // SCHED_BATCH
  THREAD_POLICY_NONE     = 0x4, 
  THREAD_POLICY_IDLE     = 0x5, // SCHED_IDLE
  THREAD_POLICY_DEADLINE = 0x6, // SCHED_DEADLINE
  THREAD_POLICY_END      = 0x7
};

const char* thread_policy_tag[] = 
{
  [THREAD_POLICY_OTHER]    = "Normal",
  [THREAD_POLICY_FIFO]     = "First-In First-out",
  [THREAD_POLICY_RR]       = "Round Robin",
  [THREAD_POLICY_BATCH]    = "Batch",
  [THREAD_POLICY_NONE]     = "",
  [THREAD_POLICY_IDLE]     = "Idle",
  [THREAD_POLICY_DEADLINE] = "Deadline",
  [THREAD_POLICY_END]      = ""
};

internal Mat3x3I64 * 
populate_matrix_sample_data(Arena *arena, uint64_t sample_size)
{
  printf("  - Starting: populate_matrix_sample_data() ");
  ASSERT(arena->size - arena->commit_pos > sample_size * sizeof(Mat3x3I64) 
    && "Arena size is not sufficient.");

  const uint8_t matrix_size = 3;
  Mat3x3I64 *ptr = 
    (Mat3x3I64*) arena_alloc(arena, sample_size * sizeof(Mat3x3I64));
  Mat3x3I64 *result = ptr;

  srand(RANDOM_NUMBER_GENERATOR_SEED);

  for(uint64_t sample_count = 0; sample_count < sample_size; sample_count++)
  {
    for (int i = 0; i < matrix_size; i++)
    {
      for (int j = 0; j < matrix_size; j++)
      {
        ptr->elements[i][j] = (uint64_t) rand() % 101;
      }
    }  

    ptr = (Mat3x3I64*)((uintptr_t) ptr + (uintptr_t) sizeof(Mat3x3I64));
  }

  printf("completed...\n");
  return result;
}

internal Vec9I64 * 
populate_vector_sample_data(Arena *arena, uint64_t sample_size)
{
  printf("  - Starting: populate_vector_sample_data() ");
  ASSERT(arena->size - arena->commit_pos > sample_size * sizeof(Vec9I64) 
    && "Arena size is not sufficient.");

  const uint8_t vector_size = 100;
  Vec9I64 *ptr = 
    (Vec9I64*) arena_alloc(arena, sample_size * sizeof(Vec9I64));
  Vec9I64 *result = ptr;

  srand(RANDOM_NUMBER_GENERATOR_SEED);

  for(uint64_t sample_count = 0; sample_count < sample_size; sample_count++)
  {
    for (int i = 0; i < vector_size; i++) 
    {
      ptr->elements[i] = (uint64_t) rand() % 101;
    }

    ptr = (Vec9I64*)((uintptr_t) ptr + (uintptr_t) sizeof(Vec9I64));
  }

  printf("completed...\n");
  return result;

}

internal fftw_complex *
populate_fft_sample_data(Arena *arena, uint64_t sample_size)
{
  printf("  - Starting: populate_fft_sample_data() ");
  ASSERT(arena->size - arena->commit_pos > sample_size * sizeof(Vec9I64) 
    && "Arena size is not sufficient.");

  fftw_complex *ptr = 
    (fftw_complex*) arena_alloc(arena, sample_size * sizeof(fftw_complex));
  fftw_complex *result = ptr;

  for(uint64_t sample_count = 0; sample_count < sample_size; sample_count++)
  {
    double t = (double) sample_count / sample_size;
    ptr[sample_count][0] = sin(2 * M_PI * 5 * t);
    ptr[sample_count][1] = 0.0; 

    ptr = (fftw_complex*)((uintptr_t) ptr + (uintptr_t) sizeof(fftw_complex));
  }

  printf("completed...\n");
  return result;
}

internal void * 
perform_test_matrix_multiplication(void *parameters)
{
  printf("  - perform_test_matrix_multiplication()\n");

  uint64_t sample_size = ((ThreadParameters*) parameters)->matrix_sample_data_size;
  Mat3x3I64 *matrix1 = ((ThreadParameters*) parameters)->matrix_sample_data;
  Mat3x3I64 *matrix2 = (Mat3x3I64*)((uintptr_t) ((ThreadParameters*) parameters)->matrix_sample_data + (uintptr_t) sizeof(Mat3x3I64));
  const uint8_t matrix_size = 3;

  Mat3x3I64 result;

  for (uint64_t sample_count = 0; sample_count < sample_size; sample_count++)
  {
    for (int i = 0; i < matrix_size; i++)
    {
      for (int j = 0; j < matrix_size; j++)
      {
        result.elements[i][j] = 0;

        for (int k = 0; k < matrix_size; k++)
        {
          result.elements[i][j] += matrix1->elements[i][k] * matrix2->elements[k][j];
        }
      }
    }

    matrix1 = (Mat3x3I64*)((uintptr_t) matrix1 + (uintptr_t) sizeof(Mat3x3I64));
    matrix2 = (Mat3x3I64*)((uintptr_t) matrix2 + (uintptr_t) sizeof(Mat3x3I64));
  }

  pthread_exit(NULL); 
}

internal void * 
perform_test_vector_dot_product(void *parameters)
{
  printf("  - perform_test_vector_multiplication()\n");

  uint64_t sample_size = ((ThreadParameters*) parameters)->vector_sample_data_size;
  Vec9I64 *vector1 = ((ThreadParameters*) parameters)->vector_sample_data;
  Vec9I64 *vector2 = (Vec9I64*)((uintptr_t) ((ThreadParameters*) parameters)->vector_sample_data + (uintptr_t) sizeof(Vec9I64));
  const uint8_t vector_size = 9;

  int64_t result;

  for (uint64_t sample_count = 0; sample_count < sample_size; sample_count++)
  {

    for (int i = 0; i < vector_size; i++) 
    {
      result += vector1->elements[i] * vector2->elements[i];
    }

    vector1 = (Vec9I64*)((uintptr_t) vector1 + (uintptr_t) sizeof(Vec9I64));
    vector2 = (Vec9I64*)((uintptr_t) vector2 + (uintptr_t) sizeof(Vec9I64));

  }

  pthread_exit(NULL); 
}

internal void *
perform_test_fast_fourier_transform(void *parameters)
{
  printf("  - perform_test_fast_fourier_transform()\n");

  fftw_plan plan;
  uint64_t sample_size = ((ThreadParameters*) parameters)->fft_sample_data_size;
  fftw_complex *data = ((ThreadParameters*) parameters)->fft_sample_data;
  fftw_complex *result = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * sample_size);

  plan = fftw_plan_dft_1d(sample_size, data, result, FFTW_FORWARD, FFTW_ESTIMATE); 

  fftw_execute(plan); 

  pthread_exit(NULL); 
}

ThreadPolicy policy;   
uint64_t *buffer; 

#define SAMPLE_DATA_MAX_AMOUNT GB(3)

#define MATRIX_SAMPLE_DATA_SIZE 13000000
#define VECTOR_SAMPLE_DATA_SIZE 13000000
#define FFT_SAMPLE_DATA_SIZE 13000000

int 
main()
{
  buffer = malloc(SAMPLE_DATA_MAX_AMOUNT);     
  ASSERT(buffer != NULL && "Couldn't request memory from operating system.");

  Arena arena;
  arena_initialize(&arena, buffer, SAMPLE_DATA_MAX_AMOUNT);

  pthread_t tasks[TASKS_AMOUNT]; 

  Mat3x3I64 *matrix_sample_data = populate_matrix_sample_data(&arena, MATRIX_SAMPLE_DATA_SIZE); 
  Vec9I64 *vector_sample_data = populate_vector_sample_data(&arena, VECTOR_SAMPLE_DATA_SIZE); 
  fftw_complex *fft_sample_data = populate_fft_sample_data(&arena, FFT_SAMPLE_DATA_SIZE); 

  ThreadParameters matrix_test_params = {
    .matrix_sample_data = matrix_sample_data,
    .matrix_sample_data_size = MATRIX_SAMPLE_DATA_SIZE
  };

  ThreadParameters vector_test_params = {
    .vector_sample_data = vector_sample_data,
    .vector_sample_data_size = VECTOR_SAMPLE_DATA_SIZE
  };

  ThreadParameters fft_test_params = {
    .fft_sample_data = fft_sample_data,
    .fft_sample_data_size = FFT_SAMPLE_DATA_SIZE
  };

  for(policy = THREAD_POLICY_OTHER; policy < THREAD_POLICY_END; policy++)
  {
    if (policy == THREAD_POLICY_NONE) continue;

    ThreadConfig task_config; 
    task_config.sched_priority = sched_get_priority_max(policy);

    pthread_setschedparam(pthread_self(), policy, &task_config);
    
    printf("(%s)\n", thread_policy_tag[policy]);
    pthread_create(&tasks[0], NULL, perform_test_matrix_multiplication,  &matrix_test_params);
    pthread_create(&tasks[1], NULL, perform_test_vector_dot_product,     &vector_test_params);
    pthread_create(&tasks[2], NULL, perform_test_fast_fourier_transform, &fft_test_params);

    for (uint8_t index = 0; index < TASKS_AMOUNT; index++)
    {
      pthread_join(tasks[index], NULL);
    }

  }

  return 0;
}
