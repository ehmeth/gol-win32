#ifndef _JOB_QUEUE_H
#define _JOB_QUEUE_H

#include <cstdint>

typedef void (*job_handler_t)(void *);

void job_queue_init();

void job_queue_push(job_handler_t handler, void * data, uint32_t data_len);

void job_queue_wait_until_done();

#endif // _JOB_QUEUE_H
