type state = {
  queue: Queue.t(unit => unit),
  is_acquired: bool,
  has_faulted: bool,
};

type t = RxAtomicData.t(state);

let create = () =>
  RxAtomicData.create({
    queue: Queue.create(),
    is_acquired: false,
    has_faulted: false,
  });

let dispose = lock =>
  RxAtomicData.update(
    l => {
      Queue.clear(l.queue);
      {...l, has_faulted: true};
    },
    lock,
  );

let wait = (lock, action) => {
  let old_state =
    RxAtomicData.update_if(
      l => !l.has_faulted,
      l => {
        Queue.add(action, l.queue);
        {...l, is_acquired: true};
      },
      lock,
    );

  let is_owner = !old_state.is_acquired;
  if (is_owner) {
    let rec loop = () => {
      let work =
        RxAtomicData.synchronize(
          l =>
            try(Some(Queue.take(l.queue))) {
            | Queue.Empty =>
              RxAtomicData.unsafe_set({...l, is_acquired: false}, lock);
              None;
            },
          lock,
        );

      switch (work) {
      | None => ()
      | Some(w) =>
        try(w()) {
        | e =>
          dispose(lock);
          raise(e);
        };
        loop();
      };
    };

    loop();
  };
};
