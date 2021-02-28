/* Internal module (see Rx.Observer)
 *
 * Implementation based on:
 * https://rx.codeplex.com/SourceControl/latest#Rx.NET/Source/System.Reactive.Core/Observer.Extensions.cs
 */

let create = (~on_completed=() => (), ~on_error=e => raise(e), on_next) => (
  on_completed,
  on_error,
  on_next,
);

module type ObserverState = {
  type state('a);

  let initial_state: unit => state('a);

  let on_completed: state('a) => state('a);

  let on_error: (exn, state('a)) => state('a);

  let on_next: ('a, state('a)) => state('a);
};

module MakeObserverWithState = (O: ObserverState, D: RxCore.MutableData) => {
  let create = () => {
    let state = D.create @@ O.initial_state();
    let update = f => {
      let s = D.get(state);
      let s' = f(s);
      D.set(s', state);
    };

    let on_completed = () => update(O.on_completed);
    let on_error = e => update @@ O.on_error(e);
    let on_next = v => update @@ O.on_next(v);
    let observer = create(~on_completed, ~on_error, on_next);
    (observer, state);
  };
};

module ObserverBase = {
  /* Original implementation:
   * https://rx.codeplex.com/SourceControl/latest#Rx.NET/Source/System.Reactive.Core/Reactive/ObserverBase.cs
   */
  let create = ((on_completed, on_error, on_next)) => {
    let is_stopped = RxAtomicData.create(false);
    let stop = () => RxAtomicData.compare_and_set(false, true, is_stopped);
    let on_completed' = () => {
      let was_stopped = stop();
      if (!was_stopped) {
        on_completed();
      };
    };

    let on_error' = e => {
      let was_stopped = stop();
      if (!was_stopped) {
        on_error(e);
      };
    };

    let on_next' = x =>
      if (!RxAtomicData.unsafe_get(is_stopped)) {
        on_next(x);
      };

    (on_completed', on_error', on_next');
  };
};

module CheckedObserver = {
  /* In the original implementation, synchronization for the observer state
   * is obtained through CAS (compare-and-swap) primitives, but in OCaml we
   * don't have a standard/portable CAS primitive, so I'm using a mutex.
   * (see https://rx.codeplex.com/SourceControl/latest#Rx.NET/Source/System.Reactive.Core/Reactive/Internal/CheckedObserver.cs)
   */
  type state =
    | Idle
    | Busy
    | Done;

  let create = ((on_completed, on_error, on_next)) => {
    let state = RxAtomicData.create(Idle);
    let check_access = () =>
      RxAtomicData.update(
        s =>
          switch (s) {
          | Idle => Busy
          | Busy => failwith("Reentrancy has been detected.")
          | Done => failwith("Observer has already terminated.")
          },
        state,
      );

    let wrap_action = (thunk, new_state) => {
      check_access();
      Utils.try_finally(thunk, () => RxAtomicData.set(new_state, state));
    };

    let on_completed' = () => wrap_action(() => on_completed(), Done);
    let on_error' = e => wrap_action(() => on_error(e), Done);
    let on_next' = x => wrap_action(() => on_next(x), Idle);
    (on_completed', on_error', on_next');
  };
};

let checked = observer => CheckedObserver.create(observer);

module SynchronizedObserver = {
  /* Original implementation:
   * https://rx.codeplex.com/SourceControl/latest#Rx.NET/Source/System.Reactive.Core/Reactive/Internal/SynchronizedObserver.cs
   */
  let create = ((on_completed, on_error, on_next)) => {
    let lock = BatRMutex.create();
    let with_lock = (f, a) => BatRMutex.synchronize(~lock, f, a);
    let on_completed' = () => with_lock(on_completed, ());
    let on_error' = e => with_lock(on_error, e);
    let on_next' = x => with_lock(on_next, x);
    (on_completed', on_error', on_next');
  };
};

let synchronize = observer => SynchronizedObserver.create(observer);

module AsyncLockObserver = {
  /* Original implementation:
   * https://rx.codeplex.com/SourceControl/latest#Rx.NET/Source/System.Reactive.Core/Reactive/Internal/AsyncLockObserver.cs
   */
  let create = ((on_completed, on_error, on_next)) => {
    let async_lock = RxAsyncLock.create();
    let with_lock = thunk => RxAsyncLock.wait(async_lock, thunk);
    let on_completed' = () => with_lock(() => on_completed());
    let on_error' = e => with_lock(() => on_error(e));
    let on_next' = x => with_lock(() => on_next(x));
    ObserverBase.create((on_completed', on_error', on_next'));
  };
};

let synchronize_async_lock = observer => AsyncLockObserver.create(observer);
