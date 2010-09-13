struct default_event_t {
   /* Indicates if this event is complete */
   volatile int complete;

   /* Cond */
   pthread_cond_t cond;
   volatile int cond_wait_count;

   /* Profiling */
   int profiling_enabled;
	struct timespec prof_submit_time;
	struct timespec prof_start_time;
	struct timespec prof_end_time;
	int prof_workerid;
};

int starpu_event_default_release(starpu_event event) {
   _starpu_event_lock(event);

   event->ref_count--;

   int free = (event->ref_count == 0 && event->ref_count_priv == 0);

   _starpu_event_unlock(event);

   if (free)
      _starpu_event_free(event);

   return 0;
}

int starpu_event_default_wait(starpu_event event) {
	if (STARPU_UNLIKELY(!_starpu_worker_may_perform_blocking_calls()))
		return -EDEADLK;

   /* We can avoid mutex locking if event is already complete */
   if (!event->complete) {

      _starpu_event_lock(event);
      
      event->cond_wait_count += 1;

      while (!event->complete) {
         pthread_cond_wait(&event->cond, &event->mutex);
      }

      event->cond_wait_count -= 1;

      _starpu_event_unlock(event);
   }

   return 0;
}

int starpu_event_default_test(starpu_event event) {
   return event->complete;
}

starpu_event _starpu_event_default_create() {
   starpu_event ev = _starpu_event_create();

   

   ev->data

   ev->complete = 0;
}
