#include "job_queue.h"
#include <windows.h>

const uint32_t JOB_QUEUE_WORKERS = 3;
const uint32_t JOB_QUEUE_SIZE = 10;
const uint32_t JOB_QUEUE_PARAMS_SIZE = 40;

enum job_status_t {
   JOB_FREE,
   JOB_AVAILABLE,
   JOB_PENDING
};

struct job_spec_t {
   job_status_t status;
   job_handler_t handler;
   uint8_t params[JOB_QUEUE_PARAMS_SIZE];
};

static struct {
   HANDLE work_event;
   CRITICAL_SECTION cs;
   bool running;
   job_spec_t specs[JOB_QUEUE_SIZE];
} job_queue;

static DWORD job_queue_worker(LPVOID param);

void job_queue_init()
{
   job_queue.work_event = CreateEvent(NULL, TRUE, FALSE, NULL);
   InitializeCriticalSection(&job_queue.cs);
   job_queue.running = true;
   for (auto i = 0; i < JOB_QUEUE_SIZE; i++)
   {
      job_queue.specs[i].handler = NULL;
      job_queue.specs[i].status = JOB_FREE;
   }

   for (auto i = 0; i < JOB_QUEUE_WORKERS; i++) {
      HANDLE thread = CreateThread( 0, 0,
            job_queue_worker, NULL,
            0, 0); 
      if (thread != NULL) {
         CloseHandle(thread);
      } else {
         OutputDebugStringA("Thread creation failed");
      }
   }
}

void job_queue_push(job_handler_t handler, void * data, uint32_t data_len)
{
   bool queued = false;

   EnterCriticalSection(&job_queue.cs);
   for (auto i = 0; (i < JOB_QUEUE_SIZE) && !queued; i++)
   {
      if (job_queue.specs[i].status == JOB_FREE) {
         job_queue.specs[i].handler = handler;
         memcpy_s(job_queue.specs[i].params, JOB_QUEUE_PARAMS_SIZE, data, data_len);
         job_queue.specs[i].status = JOB_AVAILABLE;
         queued = true;
         SetEvent(job_queue.work_event);
      }
   }
   LeaveCriticalSection(&job_queue.cs);

   if (!queued) {
      handler(data);
   }
}

void job_queue_wait_until_done()
{
   bool all_done = false;
   bool all_started = false;

   while (!all_started)
   {
      all_started = true;
      bool found = false;
      uint32_t job_index;

      EnterCriticalSection(&job_queue.cs);
      for (auto i = 0; (i < JOB_QUEUE_SIZE) && !found; i++)
      {
         if (job_queue.specs[i].status == JOB_AVAILABLE) {
            job_queue.specs[i].status = JOB_PENDING;
            found = true;
            all_started = false;
            job_index = i;
         } 
      }
      LeaveCriticalSection(&job_queue.cs);

      if (found) {
         job_queue.specs[job_index].handler(job_queue.specs[job_index].params);
         EnterCriticalSection(&job_queue.cs);
         job_queue.specs[job_index].status = JOB_FREE;
         LeaveCriticalSection(&job_queue.cs);
      }
   }

   ResetEvent(job_queue.work_event);
   while (!all_done) {
      all_done = true;
      EnterCriticalSection(&job_queue.cs);
      for (auto i = 0; (i < JOB_QUEUE_SIZE) && all_done; i++)
      {
         if (job_queue.specs[i].status != JOB_FREE) {
            all_done = false;
         } 
      }
      LeaveCriticalSection(&job_queue.cs);
   }
}

static DWORD job_queue_worker(LPVOID param) {
   while (job_queue.running)
   {
      WaitForSingleObject(job_queue.work_event, INFINITE);
      
      bool found = false;
      uint32_t job_index;
      EnterCriticalSection(&job_queue.cs);
      for (auto i = 0; (i < JOB_QUEUE_SIZE) && !found; i++)
      {
         if (job_queue.specs[i].status == JOB_AVAILABLE) {
            job_queue.specs[i].status = JOB_PENDING;
            found = true;
            job_index = i;
         }
      }
      LeaveCriticalSection(&job_queue.cs);
      if (found) {
         job_queue.specs[job_index].handler(job_queue.specs[job_index].params);
         EnterCriticalSection(&job_queue.cs);
         job_queue.specs[job_index].status = JOB_FREE;
         LeaveCriticalSection(&job_queue.cs);
      }
   }

   return 0;
}

