#include <stdio.h>
#include <stdint.h>

#include <pthread.h>
#include <linux/sched.h>

#define TASKS_AMOUNT 3

typedef struct sched_param ThreadConfig;

typedef enum ThreadPolicy ThreadPolicy;
enum ThreadPolicy {
  THREAD_POLICY_OTHER    = 0x0, // SCHED_OTHER
  THREAD_POLICY_FIFO     = 0x1, // SCHED_FIFO
  THREAD_POLICY_RR       = 0x2, // SCHED_RR
  THREAD_POLICY_BATCH    = 0x3, // SCHED_BATCH
  THREAD_POLICY_IDLE     = 0x5, // SCHED_IDLE
  THREAD_POLICY_DEADLINE = 0x6, // SCHED_DEADLINE
  THREAD_POLICY_END      = 0x7
};

const char* thread_policy_tag[] = {
  [THREAD_POLICY_OTHER]    = "Normal",
  [THREAD_POLICY_FIFO]     = "First-In First-out",
  [THREAD_POLICY_RR]       = "Round Robin",
  [THREAD_POLICY_BATCH]    = "Batch",
  [THREAD_POLICY_IDLE]     = "Idle",
  [THREAD_POLICY_DEADLINE] = "Deadline",
  [THREAD_POLICY_END]      = "",
};

static void * 
perform_test_matrix_multiplication()
{
  
}

static void * 
perform_test_vector_dot_product()
{

}

static void *
perform_test_fast_fourier_transform()
{

}

ThreadPolicy policy;   

int 
main()
{
  pthread_t tasks[TASKS_AMOUNT]; 

  for(policy = THREAD_POLICY_OTHER; policy < THREAD_POLICY_END; policy++)
  {
    ThreadConfig task_config; 
    task_config.sched_priority = sched_get_priority_max(policy);

    pthread_setschedparam(pthread_self(), policy, &task_config);
    
    pthread_create(&tasks[0], NULL, perform_test_matrix_multiplication, NULL);
    pthread_create(&tasks[1], NULL, perform_test_vector_dot_product, NULL);
    pthread_create(&tasks[2], NULL, perform_test_fast_fourier_transform, NULL);

    for (uint8_t index; index < TASKS_AMOUNT; index++)
    {
      pthread_join(tasks[index], NULL);
    }

  }

  return 0;
}

