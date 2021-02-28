/* Internal module (see Rx.Observer) */

let create:
  (~on_completed: unit => unit=?, ~on_error: exn => unit=?, 'a => unit) =>
  RxCore.observer('a);

module type ObserverState = {
  type state('a);

  let initial_state: unit => state('a);

  let on_completed: state('a) => state('a);

  let on_error: (exn, state('a)) => state('a);

  let on_next: ('a, state('a)) => state('a);
};

module MakeObserverWithState:
  (O: ObserverState, D: RxCore.MutableData) =>
   {let create: unit => (RxCore.observer('a), D.t(O.state('a)));};

let checked: RxCore.observer('a) => RxCore.observer('a);

let synchronize: RxCore.observer('a) => RxCore.observer('a);

let synchronize_async_lock: RxCore.observer('a) => RxCore.observer('a);
