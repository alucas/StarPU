
starpu_event starpu_event_create() {
   starpu_event event = _starpu_event_create();
   event->user_event = 1;
   event->ref_count = 1;
   event->ref_count_priv = 0;
   return event;
}

void starpu_event_trigger(starpu_event event) {
   assert(event->user_event);
   _starpu_event_complete(event);
}
