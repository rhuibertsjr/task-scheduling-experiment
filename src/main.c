#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <unistd.h>

#include <pthread.h>
#include <linux/sched.h>

#define ASSERT(c) do { if (!(c)) {                                                \
  printf("Assertion failed: %s\n", #c ); fflush(stdout); (*(volatile int*)0 = 0); \
} } while(0)

#define RANDOM_NUMBER_GENERATOR_SEED 64

#define TASKS_AMOUNT 3
#define MATRIX_SAMPLE_DATA_SIZE 4

#define MB(x) ((x) << 20)
#define GB(x) ((x) << 30)

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
		void *ptr = (uint64_t*) arena->memory + arena->commit_pos;
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
  ASSERT(arena->size > sample_size * sizeof(Mat3x3I64) 
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

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        printf("%lld ", ptr->elements[i][j]); // Use %lld for int64_t
      }
      printf("\n"); // Move to the next row
    }    
    printf("\n");

    ptr = (Mat3x3I64*)((uintptr_t) ptr + (uintptr_t) sizeof(Mat3x3I64));

  }

  return result;

}

internal void * 
perform_test_matrix_multiplication(void *parameters)
{
  uint64_t sample_size = ((ThreadParameters*) parameters)->matrix_sample_data_size;
  Mat3x3I64 *matrix1 = ((ThreadParameters*) parameters)->matrix_sample_data;
  Mat3x3I64 *matrix2 = ((ThreadParameters*) parameters)->matrix_sample_data;
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

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        printf("%lld ", matrix1->elements[i][j]); // Use %lld for int64_t
      }
      printf("\n"); // Move to the next row
    }    
    printf("\n");

    matrix1 = (Mat3x3I64*)((uintptr_t) matrix1 + (uintptr_t) sizeof(Mat3x3I64));
    matrix2 = (Mat3x3I64*)((uintptr_t) matrix2 + (uintptr_t) sizeof(Mat3x3I64));

  }

  pthread_exit(NULL); 
}

internal void * 
perform_test_vector_dot_product(void *parameters)
{

}

internal void *
perform_test_fast_fourier_transform(void *parameters)
{

}

ThreadPolicy policy;   
uint64_t buffer[MB(1)]; 

int 
main()
{
  Arena arena;
  arena_initialize(&arena, &buffer, MB(1));

  pthread_t tasks[TASKS_AMOUNT]; 

  Mat3x3I64 *matrix_sample_data = populate_matrix_sample_data(&arena, MATRIX_SAMPLE_DATA_SIZE); 

  ThreadParameters params = {
    matrix_sample_data,
    MATRIX_SAMPLE_DATA_SIZE
  };

  for(policy = THREAD_POLICY_OTHER; policy < THREAD_POLICY_END; policy++)
  {
    if (policy == THREAD_POLICY_NONE) continue;

    ThreadConfig task_config; 
    task_config.sched_priority = sched_get_priority_max(policy);

    pthread_setschedparam(pthread_self(), policy, &task_config);
    
    printf("(%s)\n", thread_policy_tag[policy]);
    pthread_create(&tasks[0], NULL, perform_test_matrix_multiplication,  &params);
    pthread_create(&tasks[1], NULL, perform_test_vector_dot_product,     &params);
    pthread_create(&tasks[2], NULL, perform_test_fast_fourier_transform, &params);

    for (uint8_t index; index < TASKS_AMOUNT; index++)
    {
      pthread_join(tasks[index], NULL);
    }

  }

  return 0;
}

