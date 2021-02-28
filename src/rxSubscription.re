/* Internal module. (see Rx.Subscription)
 *
 * Implementation based on:
 * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/subscriptions/Subscriptions.java
 */

let empty = () => ();

let create = unsubscribe => {
  /* Wrap the unsubscribe function in a lazy value, to get idempotency. */
  let idempotent_thunk = lazy(unsubscribe());
  () => Lazy.force(idempotent_thunk);
};

let from_task = (task, ()) => Lwt.cancel(task);

module type BooleanSubscription = {
  type state;

  let is_unsubscribed: state => bool;
};

module Boolean = {
  /* Implementation based on:
   * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/subscriptions/BooleanSubscription.java
   */
  type state = RxAtomicData.t(bool);

  let create = unsubscribe => {
    let is_unsubscribed = RxAtomicData.create(false);
    let unsubscribe_wrapper = () => {
      let was_unsubscribed =
        RxAtomicData.compare_and_set(false, true, is_unsubscribed);
      if (!was_unsubscribed) {
        unsubscribe();
      };
    };

    (unsubscribe_wrapper, is_unsubscribed);
  };

  let is_unsubscribed = state => RxAtomicData.unsafe_get(state);
};

module Composite = {
  /* Implementation based on:
   * https://github.com/Netflix/RxJava/blob/master/rxjava-core/src/main/java/rx/subscriptions/CompositeSubscription.java
   */
  exception CompositeException(list(exn));

  module State = {
    type t = {
      is_unsubscribed: bool,
      subscriptions: list(RxCore.subscription),
    };

    let unsubscribe = state => {
      is_unsubscribed: true,
      subscriptions: state.subscriptions,
    };

    let add = (state, subscription) => {
      is_unsubscribed: state.is_unsubscribed,
      subscriptions: state.subscriptions @ [subscription],
    };

    let remove = (state, subscription) => {
      is_unsubscribed: state.is_unsubscribed,
      subscriptions:
        List.filter(s => s !== subscription, state.subscriptions),
    };

    let clear = state => {
      is_unsubscribed: state.is_unsubscribed,
      subscriptions: [],
    };
  };

  type state = RxAtomicData.t(State.t);

  let unsubscribe_from_all = subscriptions => {
    let exceptions =
      List.fold_left(
        (exns, unsubscribe) =>
          try(
            {
              unsubscribe();
              exns;
            }
          ) {
          | e => [e, ...exns]
          },
        [],
        subscriptions,
      );

    if (List.length(exceptions) > 0) {
      raise(CompositeException(exceptions));
    };
  };

  let create = subscriptions => {
    let state =
      RxAtomicData.create({State.is_unsubscribed: false, subscriptions});
    let unsubscribe_wrapper = () => {
      let old_state =
        RxAtomicData.update_if(
          s => !s.State.is_unsubscribed,
          s => State.unsubscribe(s),
          state,
        );

      let was_unsubscribed = old_state.State.is_unsubscribed;
      let subscriptions = old_state.State.subscriptions;
      if (!was_unsubscribed) {
        unsubscribe_from_all(subscriptions);
      };
    };

    (unsubscribe_wrapper, state);
  };

  let is_unsubscribed = state =>
    RxAtomicData.unsafe_get(state).State.is_unsubscribed;

  let add = (state, subscription) => {
    let old_state =
      RxAtomicData.update_if(
        s => !s.State.is_unsubscribed,
        s => State.add(s, subscription),
        state,
      );

    if (old_state.State.is_unsubscribed) {
      subscription();
    };
  };

  let remove = (state, subscription) => {
    let old_state =
      RxAtomicData.update_if(
        s => !s.State.is_unsubscribed,
        s => State.remove(s, subscription),
        state,
      );

    if (!old_state.State.is_unsubscribed) {
      subscription();
    };
  };

  let clear = state => {
    let old_state =
      RxAtomicData.update_if(
        s => !s.State.is_unsubscribed,
        s => State.clear(s),
        state,
      );

    let was_unsubscribed = old_state.State.is_unsubscribed;
    let subscriptions = old_state.State.subscriptions;
    if (!was_unsubscribed) {
      unsubscribe_from_all(subscriptions);
    };
  };
};

module Assignable = {
  module State = {
    type t = {
      is_unsubscribed: bool,
      subscription: option(RxCore.subscription),
    };

    let unsubscribe = state => {
      is_unsubscribed: true,
      subscription: state.subscription,
    };

    let set = (state, subscription) => {
      is_unsubscribed: state.is_unsubscribed,
      subscription: Some(subscription),
    };
  };

  type state = RxAtomicData.t(State.t);

  let is_unsubscribed = state => {
    let s = RxAtomicData.unsafe_get(state);
    s.State.is_unsubscribed;
  };

  let create = (~subscription=?, ()) => {
    let state =
      RxAtomicData.create({State.is_unsubscribed: false, State.subscription});
    let unsubscribe_wrapper = () => {
      let old_state =
        RxAtomicData.update_if(
          s => !s.State.is_unsubscribed,
          s => State.unsubscribe(s),
          state,
        );

      let was_unsubscribed = old_state.State.is_unsubscribed;
      let subscription =
        BatOption.default(empty, old_state.State.subscription);
      if (!was_unsubscribed) {
        subscription();
      };
    };

    (unsubscribe_wrapper, state);
  };

  let set = (update, state, subscription) => {
    let old_state = update(state);
    let was_unsubscribed = old_state.State.is_unsubscribed;
    if (was_unsubscribed) {
      subscription();
    };
  };
};

module MultipleAssignment = {
  include Assignable;

  let create = subscription => create(~subscription, ());

  let set = (state, subscription) =>
    set(
      RxAtomicData.update_if(
        s => !s.State.is_unsubscribed,
        s => State.set(s, subscription),
      ),
      state,
      subscription,
    );
};

module SingleAssignment = {
  include Assignable;

  let create = () => create();

  let set = (state, subscription) =>
    set(
      RxAtomicData.update_if(
        s => !s.State.is_unsubscribed,
        s =>
          switch (s.State.subscription) {
          | None => State.set(s, subscription)
          | Some(_) => failwith("SingleAssignment")
          },
      ),
      state,
      subscription,
    );
};
