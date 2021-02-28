/* Internal module. (see Rx.Observable)
 *
 * Implementation based on:
 * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/Observable.java
 */

let empty = ((on_completed, _, _)) => {
  on_completed();
  RxSubscription.empty;
};

let error = (e, (_, on_error, _)) => {
  on_error(e);
  RxSubscription.empty;
};

let never = ((_, _, _)) => RxSubscription.empty;

let return = (v, (on_completed, on_error, on_next)) => {
  on_next(v);
  on_completed();
  RxSubscription.empty;
};

let materialize =
    (
      observable,
      /* Implementation based on:
       * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/operators/OperationMaterialize.java
       */
      (on_completed, on_error, on_next),
    ) => {
  let materialize_observer =
    RxObserver.create(
      ~on_completed=
        () => {
          on_next(RxCore.OnCompleted);
          on_completed();
        },
      ~on_error=
        e => {
          on_next(RxCore.OnError(e));
          on_completed();
        },
      v => on_next(RxCore.OnNext(v)),
    );

  observable(materialize_observer);
};

let dematerialize =
    (
      observable,
      /* Implementation based on:
       * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/operators/OperationDematerialize.java
       */
      (on_completed, on_error, on_next),
    ) => {
  let materialize_observer =
    RxObserver.create(
      ~on_completed=ignore,
      ~on_error=ignore,
      fun
      | RxCore.OnCompleted => on_completed()
      | RxCore.OnError(e) => on_error(e)
      | RxCore.OnNext(v) => on_next(v),
    );

  observable(materialize_observer);
};

let length =
    (
      observable,
      /* Implementation based on:
       * https://rx.codeplex.com/SourceControl/latest#Rx.NET/Source/System.Reactive.Linq/Reactive/Linq/Observable/Count.cs
       */
      (on_completed, on_error, on_next),
    ) => {
  let counter = RxAtomicData.create(0);
  let length_observer =
    RxObserver.create(
      ~on_completed=
        () => {
          let v = RxAtomicData.unsafe_get(counter);
          on_next(v);
          on_completed();
        },
      ~on_error,
      _ => RxAtomicData.update(succ, counter),
    );

  observable(length_observer);
};

let drop =
    (
      n,
      observable,
      /* Implementation based on:
       * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/operators/OperationSkip.java
       */
      (on_completed, on_error, on_next),
    ) => {
  let counter = RxAtomicData.create(0);
  let drop_observer =
    RxObserver.create(
      ~on_completed,
      ~on_error,
      v => {
        let count = RxAtomicData.update_and_get(succ, counter);
        if (count > n) {
          on_next(v);
        };
      },
    );

  observable(drop_observer);
};

let take =
    (
      n,
      observable,
      /* Implementation based on:
       * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/operators/OperationTake.java
       */
      (on_completed, on_error, on_next),
    ) =>
  if (n < 1) {
    let observer =
      RxObserver.create(~on_completed=ignore, ~on_error=ignore, ignore);
    let unsubscribe = observable(observer);
    unsubscribe();
    RxSubscription.empty;
  } else {
    let counter = RxAtomicData.create(0);
    let error = ref(false);
    let (unsubscribe, subscription_state) =
      RxSubscription.SingleAssignment.create();
    let take_observer = {
      let on_completed_wrapper = () =>
        if (! error^ && RxAtomicData.get_and_set(n, counter) < n) {
          on_completed();
        };
      let on_error_wrapper = e =>
        if (! error^ && RxAtomicData.get_and_set(n, counter) < n) {
          on_error(e);
        };

      RxObserver.create(
        ~on_completed=on_completed_wrapper, ~on_error=on_error_wrapper, v =>
        if (! error^) {
          let count = RxAtomicData.update_and_get(succ, counter);
          if (count <= n) {
            try(on_next(v)) {
            | e =>
              error := true;
              on_error(e);
              unsubscribe();
            };
            if (! error^ && count == n) {
              on_completed();
            };
          };
          if (! error^ && count >= n) {
            unsubscribe();
          };
        }
      );
    };
    let result = observable(take_observer);
    RxSubscription.SingleAssignment.set(subscription_state, result);
    result;
  };

let take_last =
    (
      n,
      observable,
      /* Implementation based on:
       * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/operators/OperationTakeLast.java
       */
      (on_completed, on_error, on_next),
    ) => {
  let queue = Queue.create();
  let (unsubscribe, subscription_state) =
    RxSubscription.SingleAssignment.create();
  let take_last_observer =
    RxObserver.create(
      ~on_completed=
        () =>
          try(
            {
              Queue.iter(on_next, queue);
              on_completed();
            }
          ) {
          | e => on_error(e)
          },
      ~on_error,
      v =>
        if (n > 0) {
          try(
            BatMutex.synchronize(
              () => {
                Queue.add(v, queue);
                if (Queue.length(queue) > n) {
                  ignore(Queue.take(queue));
                };
              },
              (),
            )
          ) {
          | e =>
            on_error(e);
            unsubscribe();
          };
        },
    );

  let result = observable(take_last_observer);
  RxSubscription.SingleAssignment.set(subscription_state, result);
  result;
};

let single =
    (
      observable,
      /* Implementation based on:
       * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/operators/OperationSingle.java
       */
      (on_completed, on_error, on_next),
    ) => {
  let value = ref(None);
  let has_too_many_elements = ref(false);
  let (unsubscribe, subscription_state) =
    RxSubscription.SingleAssignment.create();
  let single_observer =
    RxObserver.create(
      ~on_completed=
        () =>
          if (! has_too_many_elements^) {
            switch (value^) {
            | None => on_error(Failure("Sequence contains no elements"))
            | Some(v) =>
              on_next(v);
              on_completed();
            };
          },
      ~on_error,
      v =>
        switch (value^) {
        | None => value := Some(v)
        | Some(_) =>
          has_too_many_elements := true;
          on_error(Failure("Sequence contains too many elements"));
          unsubscribe();
        },
    );

  let result = observable(single_observer);
  RxSubscription.SingleAssignment.set(subscription_state, result);
  result;
};

let append = (o1, o2, (on_completed, on_error, on_next) as o2_observer) => {
  let o1_observer =
    RxObserver.create(
      ~on_completed=() => ignore @@ o2(o2_observer),
      ~on_error,
      on_next,
    );

  o1(o1_observer);
};

let merge =
    (
      observables,
      /* Implementation based on:
       * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/operators/OperationMerge.java
       */
      actual_observer,
    ) => {
  let (on_completed, on_error, on_next) =
    RxObserver.synchronize(actual_observer);
  let (unsubscribe, subscription_state) =
    RxSubscription.Composite.create([]);
  let is_stopped = RxAtomicData.create(false);
  let child_counter = RxAtomicData.create(0);
  let parent_completed = ref(false);
  let stop = () => {
    let was_stopped = RxAtomicData.compare_and_set(false, true, is_stopped);
    if (!was_stopped) {
      unsubscribe();
    };
    was_stopped;
  };
  let stop_if_and_do = (cond, thunk) =>
    if (!RxAtomicData.get(is_stopped) && cond) {
      let was_stopped = stop();
      if (!was_stopped) {
        thunk();
      };
    };
  let child_observer =
    RxObserver.create(
      ~on_completed=
        () => {
          let count = RxAtomicData.update_and_get(pred, child_counter);
          stop_if_and_do(count == 0 && parent_completed^, on_completed);
        },
      ~on_error=e => stop_if_and_do(true, () => on_error(e)),
      v =>
        if (!RxAtomicData.get(is_stopped)) {
          on_next(v);
        },
    );
  let parent_observer =
    RxObserver.create(
      ~on_completed=
        () => {
          parent_completed := true;
          let count = RxAtomicData.get(child_counter);
          stop_if_and_do(count == 0, on_completed);
        },
      ~on_error,
      observable =>
        if (!RxAtomicData.get(is_stopped)) {
          RxAtomicData.update(succ, child_counter);
          let child_subscription = observable(child_observer);
          RxSubscription.Composite.add(
            subscription_state,
            child_subscription,
          );
        },
    );
  let parent_subscription = observables(parent_observer);
  let (subscription, _) =
    RxSubscription.Composite.create([parent_subscription, unsubscribe]);

  subscription;
};

let map = (f, observable, (on_completed, on_error, on_next)) => {
  let map_observer =
    RxObserver.create(~on_completed, ~on_error, v => on_next @@ f(v));

  observable(map_observer);
};

let bind = (observable, f) => map(f, observable) |> merge;

module Blocking = {
  /* Implementation based on:
   * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/observables/BlockingObservable.java
   */

  let to_enum = observable => {
    /* Implementation based on:
     * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/operators/OperationToIterator.java
     */
    let condition = Condition.create();
    let mutex = Mutex.create();
    let queue = Queue.create();
    let observer =
      RxObserver.create(~on_completed=ignore, ~on_error=ignore, n =>
        BatMutex.synchronize(
          ~lock=mutex,
          () => {
            Queue.add(n, queue);
            Condition.signal(condition);
          },
          (),
        )
      );
    let _ = materialize(observable, observer);
    BatEnum.from(() =>
      BatMutex.synchronize(
        ~lock=mutex,
        () => {
          if (Queue.is_empty(queue)) {
            Condition.wait(condition, mutex);
          };
          let n = Queue.take(queue);
          switch (n) {
          | RxCore.OnCompleted => raise(BatEnum.No_more_elements)
          | RxCore.OnError(e) => raise(e)
          | RxCore.OnNext(v) => v
          };
        },
        (),
      )
    );
  };

  let single = observable => {
    let enum = single(observable) |> to_enum;
    BatEnum.get_exn(enum);
  };
};

module type Scheduled = {
  let subscribe_on_this: RxCore.observable('a) => RxCore.observable('a);

  let of_enum: BatEnum.t('a) => RxCore.observable('a);

  let interval: float => RxCore.observable(int);
};

module MakeScheduled = (Scheduler: RxScheduler.S) => {
  let subscribe_on_this = (observable, observer) =>
    Scheduler.schedule_absolute(() =>{
      let unsubscribe = observable(observer);
      RxSubscription.create(() =>
        ignore @@
        Scheduler.schedule_absolute(() => {
          unsubscribe();
          RxSubscription.empty;
        })
      );
    });

  let of_enum = (enum, (on_completed, on_error, on_next)) =>
    Scheduler.schedule_recursive(self =>
      try({
        let elem = BatEnum.get(enum);
        switch (elem) {
        | None =>
          on_completed();
          RxSubscription.empty;
        | Some(x) =>
          on_next(x);
          self();
        };
      }) {
      | e =>
        on_error(e);
        RxSubscription.empty;
      }
    );

  let interval =
      (
        period,
        /* Implementation based on:
         * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/operators/OperationInterval.java
         */
        (on_completed, on_error, on_next),
      ) => {
    let counter = RxAtomicData.create(0);
    Scheduler.schedule_periodically(
      ~initial_delay=period,
      period,
      () => {
        on_next(RxAtomicData.unsafe_get(counter));
        RxAtomicData.update(succ, counter);
        RxSubscription.empty;
      },
    );
  };
};

module CurrentThread = MakeScheduled(RxScheduler.CurrentThread);

module Immediate = MakeScheduled(RxScheduler.Immediate);

module NewThread = MakeScheduled(RxScheduler.NewThread);

module Lwt = MakeScheduled(RxScheduler.Lwt);

module Test = MakeScheduled(RxScheduler.Test);
