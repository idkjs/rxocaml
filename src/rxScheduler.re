/* Internal module. (see Rx.Scheduler)
 *
 * Implementation based on:
 * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/Scheduler.java
 */

module type Base = {
  type t;

  let now: unit => float;

  let schedule_absolute:
    (~due_time: float=?, unit => RxCore.subscription) => RxCore.subscription;
};

module type S = {
  include Base;

  let schedule_relative:
    (float, unit => RxCore.subscription) => RxCore.subscription;

  let schedule_recursive:
    ((unit => RxCore.subscription) => RxCore.subscription) =>
    RxCore.subscription;

  let schedule_periodically:
    (~initial_delay: float=?, float, unit => RxCore.subscription) =>
    RxCore.subscription;
};

module MakeScheduler = (BaseScheduler: Base) => {
  include BaseScheduler;

  let schedule_relative = (delay, action) => {
    let due_time = BaseScheduler.now() +. delay;
    BaseScheduler.schedule_absolute(~due_time, action);
  };

  let schedule_recursive = cont => {
    open RxSubscription;
    let (child_subscription, child_state) = MultipleAssignment.create(empty);
    let (parent_subscription, parent_state) =
      Composite.create([child_subscription]);
    let rec schedule_k = k => {
      let k_subscription =
        if (Composite.is_unsubscribed(parent_state)) {
          empty;
        } else {
          BaseScheduler.schedule_absolute(() => k(() => schedule_k(k)));
        };

      MultipleAssignment.set(child_state, k_subscription);
      child_subscription;
    };
    let scheduled_subscription =
      BaseScheduler.schedule_absolute(() => schedule_k(cont));

    Composite.add(parent_state, scheduled_subscription);
    parent_subscription;
  };

  let schedule_periodically = (~initial_delay=?, period, action) => {
    let completed = RxAtomicData.create(false);
    let rec loop = () =>
      if (!RxAtomicData.unsafe_get(completed)) {
        let started_at = BaseScheduler.now();
        let unsubscribe1 = action();
        let time_taken = now() -. started_at;
        let delay = period -. time_taken;
        let unsubscribe2 = schedule_relative(delay, loop);
        RxSubscription.create(() => {
          unsubscribe1();
          unsubscribe2();
        });
      } else {
        RxSubscription.empty;
      };

    let delay = BatOption.default(0., initial_delay);
    let unsubscribe = schedule_relative(delay, loop);
    RxSubscription.create(() => {
      RxAtomicData.set(true, completed);
      unsubscribe();
    });
  };
};

let create_sleeping_action = (action, exec_time, now, ()) => {
  if (exec_time > now()) {
    let delay = exec_time -. now();
    if (delay > 0.0) {
      Thread.delay(delay);
    };
  };
  action();
};

module DiscardableAction = {
  type t = {
    ready: bool,
    unsubscribe: RxCore.subscription,
  };

  let create_state = () => {
    let state =
      RxAtomicData.create({ready: true, unsubscribe: RxSubscription.empty});
    RxAtomicData.update(
      s =>
        {
          ...s,
          unsubscribe: () =>
            RxAtomicData.update(s' => {...s', ready: false}, state),
        },
      state,
    );
    state;
  };

  let was_ready = state => {
    let old_state =
      RxAtomicData.update_if(
        s => s.ready == true,
        s => {...s, ready: false},
        state,
      );

    old_state.ready;
  };

  let create = action => {
    let state = create_state();
    (
      () =>
        if (was_ready(state)) {
          let unsubscribe = action();
          RxAtomicData.update(s => {...s, unsubscribe}, state);
        },
      RxAtomicData.unsafe_get(state).unsubscribe,
    );
  };

  let create_lwt = action => {
    let state = create_state();
    let was_ready_thread = Lwt.wrap(() => was_ready(state));
    (
      Lwt.bind(was_ready_thread, was_ready =>
        if (was_ready) {
          Lwt.bind(
            action,
            unsubscribe => {
              RxAtomicData.update(s => {...s, unsubscribe}, state);
              Lwt.return_unit;
            },
          );
        } else {
          Lwt.return_unit;
        }
      ),
      RxAtomicData.unsafe_get(state).unsubscribe,
    );
  };
};

module TimedAction = {
  type t = {
    discardable_action: unit => unit,
    exec_time: float,
    count: int,
  };

  let compare = (ta1, ta2) => {
    let result = compare(ta1.exec_time, ta2.exec_time);
    if (result == 0) {
      compare(ta1.count, ta2.count);
    } else {
      result;
    };
  };
};

module TimedActionPriorityQueue = BatHeap.Make(TimedAction);

module CurrentThreadBase = {
  type t = {
    queue_table: Hashtbl.t(int, option(TimedActionPriorityQueue.t)),
    counter: int,
  };

  let current_state =
    RxAtomicData.create({queue_table: Hashtbl.create(16), counter: 0});

  let now = () => Unix.gettimeofday();

  let get_queue = state => {
    let tid = Utils.current_thread_id();
    let queue_table = state.queue_table;
    try(Hashtbl.find(queue_table, tid)) {
    | Not_found =>
      let queue = None;
      Hashtbl.add(queue_table, tid, queue);
      queue;
    };
  };

  let set_queue = (queue, state) => {
    let tid = Utils.current_thread_id();
    Hashtbl.replace(state.queue_table, tid, queue);
  };

  let enqueue = (action, exec_time) => {
    let exec =
      RxAtomicData.synchronize(
        state => {
          let queue_option = get_queue(state);
          let (exec, queue) =
            switch (queue_option) {
            | None => (true, TimedActionPriorityQueue.empty)
            | Some(q) => (false, q)
            };
          let queue' =
            TimedActionPriorityQueue.insert(
              queue,
              {
                TimedAction.discardable_action: action,
                exec_time,
                count: state.counter,
              },
            );
          RxAtomicData.unsafe_set(
            {...state, counter: succ(state.counter)},
            current_state,
          );
          set_queue(Some(queue'), state);
          exec;
        },
        current_state,
      );
    let reset_queue = () =>
      RxAtomicData.synchronize(
        state => set_queue(None, state),
        current_state,
      );
    if (exec) {
      try(
        while (true) {
          let action =
            RxAtomicData.synchronize(
              state => {
                let queue = BatOption.get(get_queue(state));
                let result = TimedActionPriorityQueue.find_min(queue);
                let queue' = TimedActionPriorityQueue.del_min(queue);
                set_queue(Some(queue'), state);
                result.TimedAction.discardable_action;
              },
              current_state,
            );
          action();
        }
      ) {
      | Invalid_argument("find_min") => reset_queue()
      | e =>
        reset_queue();
        raise(e);
      };
    };
  };

  let schedule_absolute = (~due_time=?, action) => {
    let (exec_time, action') =
      switch (due_time) {
      | None => (now(), action)
      | Some(dt) => (dt, create_sleeping_action(action, dt, now))
      };
    let (discardable_action, unsubscribe) =
      DiscardableAction.create(action');
    enqueue(discardable_action, exec_time);
    unsubscribe;
  };
};

module CurrentThread = MakeScheduler(CurrentThreadBase);

module ImmediateBase = {
  /* Implementation based on:
   * /usr/local/src/RxJava/rxjava-core/src/main/java/rx/schedulers/ImmediateScheduler.java
   */
  type t = unit;

  let now = () => Unix.gettimeofday();

  let schedule_absolute = (~due_time=?, action) => {
    let (exec_time, action') =
      switch (due_time) {
      | None => (now(), action)
      | Some(dt) => (dt, create_sleeping_action(action, dt, now))
      };
    action'();
  };
};

module Immediate = MakeScheduler(ImmediateBase);

module NewThreadBase = {
  type t = unit;

  let now = () => Unix.gettimeofday();

  let schedule_absolute = (~due_time=?, action) => {
    let (exec_time, action') =
      switch (due_time) {
      | None => (now(), action)
      | Some(dt) => (dt, create_sleeping_action(action, dt, now))
      };
    let (discardable_action, unsubscribe) =
      DiscardableAction.create(action');
    let _ = Thread.create(discardable_action, ());
    unsubscribe;
  };
};

module NewThread = MakeScheduler(NewThreadBase);

module LwtBase = {
  type t = unit;

  let now = () => Unix.gettimeofday();

  let create_sleeping_action = (action, exec_time) => {
    let delay_action =
      if (exec_time > now()) {
        let delay = exec_time -. now();
        if (delay > 0.0) {
          Lwt_unix.sleep(delay);
        } else {
          Lwt.return_unit;
        };
      } else {
        Lwt.return_unit;
      };
    Lwt.bind(delay_action, () => Lwt.wrap(action));
  };

  let schedule_absolute = (~due_time=?, action) => {
    let (exec_time, action') =
      switch (due_time) {
      | None => (now(), Lwt.wrap(action))
      | Some(dt) => (dt, create_sleeping_action(action, dt))
      };
    let (discardable_action, unsubscribe) =
      DiscardableAction.create_lwt(action');
    let (waiter, wakener) = Lwt.task();
    let lwt_unsubscribe = RxSubscription.from_task(waiter);
    let _ = Lwt.bind(waiter, () => discardable_action);
    let () = Lwt.wakeup_later(wakener, ());
    () => {
      lwt_unsubscribe();
      unsubscribe();
    };
  };
};

module Lwt = MakeScheduler(LwtBase);

module TestBase = {
  /* Implementation based on:
   * /usr/local/src/RxJava/rxjava-core/src/main/java/rx/schedulers/TestScheduler.java
   */

  type t = {
    mutable queue: TimedActionPriorityQueue.t,
    mutable time: float,
  };

  let current_state = {queue: TimedActionPriorityQueue.empty, time: 0.0};

  let now = () => current_state.time;

  let schedule_absolute = (~due_time=?, action) => {
    let exec_time =
      switch (due_time) {
      | None => now()
      | Some(dt) => dt
      };
    let (discardable_action, unsubscribe) = DiscardableAction.create(action);
    let queue =
      TimedActionPriorityQueue.insert(
        current_state.queue,
        {TimedAction.discardable_action, exec_time, count: 0},
      );
    current_state.queue = queue;
    unsubscribe;
  };

  let trigger_actions = target_time => {
    let rec loop = () =>
      try({
        let timed_action =
          TimedActionPriorityQueue.find_min(current_state.queue);
        if (timed_action.TimedAction.exec_time <= target_time) {
          let queue = TimedActionPriorityQueue.del_min(current_state.queue);
          current_state.time = timed_action.TimedAction.exec_time;
          current_state.queue = queue;
          timed_action.TimedAction.discardable_action();
          loop();
        };
      }) {
      | Invalid_argument("find_min") => ()
      };

    loop();
  };

  let trigger_actions_until_now = () => trigger_actions(current_state.time);

  let advance_time_to = delay => {
    current_state.time = delay;
    trigger_actions(delay);
  };

  let advance_time_by = delay => {
    let target_time = current_state.time +. delay;
    trigger_actions(target_time);
  };
};

module Test = {
  include MakeScheduler(TestBase);

  let now = TestBase.now;

  let trigger_actions = TestBase.trigger_actions;

  let trigger_actions_until_now = TestBase.trigger_actions_until_now;

  let advance_time_to = TestBase.advance_time_to;

  let advance_time_by = TestBase.advance_time_by;
};
