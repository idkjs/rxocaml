/* Internal module. (see Rx.Subject)
 *
 * Implementation based on:
 * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/subjects/Subject.java
 */

let unsubscribe_observer = (observer, observers) =>
  List.filter(o => o !== observer, observers);

let create = () => {
  /* Implementation based on:
   * https://rx.codeplex.com/SourceControl/latest#Rx.NET/Source/System.Reactive.Linq/Reactive/Subjects/Subject.cs
   */
  let observers = RxAtomicData.create([]);
  let update = f => RxAtomicData.update(f, observers);
  let sync = f => RxAtomicData.synchronize(f, observers);
  let iter = f => sync(os => List.iter(f, os));
  let observable = observer => {
    let _ = update(os => [observer, ...os]);
    () => update(unsubscribe_observer(observer));
  };
  let observer =
    RxObserver.create(
      ~on_completed=() => iter(((on_completed, _, _)) => on_completed()),
      ~on_error=e => iter(((_, on_error, _)) => on_error(e)),
      v => iter(((_, _, on_next)) => on_next(v)),
    );
  let unsubscribe = () => RxAtomicData.set([], observers);
  ((observer, observable), unsubscribe);
};

module Replay = {
  type t('a) = {
    queue: Queue.t(RxCore.notification('a)),
    is_stopped: bool,
    observers: list(RxCore.observer('a)),
  };

  let create = () => {
    /* Implementation based on:
     * https://rx.codeplex.com/SourceControl/latest#Rx.NET/Source/System.Reactive.Linq/Reactive/Subjects/ReplaySubject.cs
     * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/subjects/ReplaySubject.java
     */
    let state =
      RxAtomicData.create({
        queue: Queue.create(),
        is_stopped: false,
        observers: [],
      });
    let update = f => RxAtomicData.update(f, state);
    let sync = f => RxAtomicData.synchronize(f, state);
    let if_not_stopped = f =>
      sync(s =>
        if (!s.is_stopped) {
          f(s);
        }
      );
    let observable = ((on_completed, on_error, on_next) as observer) => {
      let _ =
        sync(s =>{
          let observers = [observer, ...s.observers];
          RxAtomicData.unsafe_set({...s, observers}, state);
          Queue.iter(
            fun
            | RxCore.OnCompleted => on_completed()
            | RxCore.OnError(e) => on_error(e)
            | RxCore.OnNext(v) => on_next(v),
            s.queue,
          );
        });

      () =>
        update(s =>
          {...s, observers: unsubscribe_observer(observer, s.observers)}
        );
    };
    let observer =
      RxObserver.create(
        ~on_completed=
          () =>
            if_not_stopped(s => {
              RxAtomicData.unsafe_set({...s, is_stopped: true}, state);
              Queue.add(RxCore.OnCompleted, s.queue);
              List.iter(
                ((on_completed, _, _)) => on_completed(),
                s.observers,
              );
            }),
        ~on_error=
          e =>
            if_not_stopped(s => {
              RxAtomicData.unsafe_set({...s, is_stopped: true}, state);
              Queue.add(RxCore.OnError(e), s.queue);
              List.iter(((_, on_error, _)) => on_error(e), s.observers);
            }),
        v =>
          if_not_stopped(s => {
            Queue.add(RxCore.OnNext(v), s.queue);
            List.iter(((_, _, on_next)) => on_next(v), s.observers);
          }),
      );
    let unsubscribe = () => update(s => {...s, observers: []});
    ((observer, observable), unsubscribe);
  };
};

module Behavior = {
  type t('a) = {
    last_notification: RxCore.notification('a),
    observers: list(RxCore.observer('a)),
  };

  let create = default_value => {
    /* Implementation based on:
     * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/subjects/BehaviorSubject.java
     */
    let state =
      RxAtomicData.create({
        last_notification: RxCore.OnNext(default_value),
        observers: [],
      });
    let update = f => RxAtomicData.update(f, state);
    let sync = f => RxAtomicData.synchronize(f, state);
    let observable = ((on_completed, on_error, on_next) as observer) => {
      let _ =
        sync(s =>{
          let observers = [observer, ...s.observers];
          RxAtomicData.unsafe_set({...s, observers}, state);
          switch (s.last_notification) {
          | RxCore.OnCompleted => on_completed()
          | RxCore.OnError(e) => on_error(e)
          | RxCore.OnNext(v) => on_next(v)
          };
        });

      () =>
        update(s =>
          {...s, observers: unsubscribe_observer(observer, s.observers)}
        );
    };
    let observer =
      RxObserver.create(
        ~on_completed=
          () =>
            sync(s => {
              RxAtomicData.unsafe_set(
                {...s, last_notification: RxCore.OnCompleted},
                state,
              );
              List.iter(
                ((on_completed, _, _)) => on_completed(),
                s.observers,
              );
            }),
        ~on_error=
          e =>
            sync(s => {
              RxAtomicData.unsafe_set(
                {...s, last_notification: RxCore.OnError(e)},
                state,
              );
              List.iter(((_, on_error, _)) => on_error(e), s.observers);
            }),
        v =>
          sync(s =>
            switch (s.last_notification) {
            | RxCore.OnNext(_) =>
              RxAtomicData.unsafe_set(
                {...s, last_notification: RxCore.OnNext(v)},
                state,
              );
              List.iter(((_, _, on_next)) => on_next(v), s.observers);
            | _ => ()
            }
          ),
      );
    let unsubscribe = () => update(s => {...s, observers: []});
    ((observer, observable), unsubscribe);
  };
};

module Async = {
  type t('a) = {
    last_notification: option(RxCore.notification('a)),
    is_stopped: bool,
    observers: list(RxCore.observer('a)),
  };

  let emit_last_notification = ((on_completed, on_error, on_next), s) =>
    switch (s.last_notification) {
    | Some(RxCore.OnCompleted) =>
      failwith(
        "Bug in AsyncSubject: should not store RxCore.OnCompleted as last notificaition",
      )
    | Some(RxCore.OnError(e)) => on_error(e)
    | Some(RxCore.OnNext(v)) =>
      on_next(v);
      on_completed();
    | None => ()
    };

  let create = () => {
    /* Implementation based on:
     * https://rx.codeplex.com/SourceControl/latest#Rx.NET/Source/System.Reactive.Linq/Reactive/Subjects/AsyncSubject.cs
     * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/subjects/AsyncSubject.java
     */
    let state =
      RxAtomicData.create({
        last_notification: None,
        is_stopped: false,
        observers: [],
      });
    let update = f => RxAtomicData.update(f, state);
    let sync = f => RxAtomicData.synchronize(f, state);
    let if_not_stopped = f =>
      sync(s =>
        if (!s.is_stopped) {
          f(s);
        }
      );
    let observable = observer => {
      let _ =
        sync(s =>{
          let observers = [observer, ...s.observers];
          RxAtomicData.unsafe_set({...s, observers}, state);
          if (s.is_stopped) {
            emit_last_notification(observer, s);
          };
        });

      () =>
        update(s =>
          {...s, observers: unsubscribe_observer(observer, s.observers)}
        );
    };
    let observer =
      RxObserver.create(
        ~on_completed=
          () =>
            if_not_stopped(s => {
              RxAtomicData.unsafe_set({...s, is_stopped: true}, state);
              List.iter(
                observer => emit_last_notification(observer, s),
                s.observers,
              );
            }),
        ~on_error=
          e =>
            if_not_stopped(s => {
              RxAtomicData.unsafe_set(
                {
                  ...s,
                  is_stopped: true,
                  last_notification: Some(RxCore.OnError(e)),
                },
                state,
              );
              List.iter(((_, on_error, _)) => on_error(e), s.observers);
            }),
        v =>
          if_not_stopped(s =>
            RxAtomicData.unsafe_set(
              {...s, last_notification: Some(RxCore.OnNext(v))},
              state,
            )
          ),
      );
    let unsubscribe = () => update(s => {...s, observers: []});
    ((observer, observable), unsubscribe);
  };
};
