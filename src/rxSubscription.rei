/* Internal module (see Rx.Subscription) */

let empty: RxCore.subscription;

let create: (unit => unit) => RxCore.subscription;

let from_task: Lwt.t('a) => RxCore.subscription;

module type BooleanSubscription = {
  type state;

  let is_unsubscribed: state => bool;
};

module Boolean: {
  include BooleanSubscription;

  let create: (unit => unit) => (RxCore.subscription, state);
};

module Composite: {
  exception CompositeException(list(exn));

  include BooleanSubscription;

  let create: list(RxCore.subscription) => (RxCore.subscription, state);

  let add: (state, RxCore.subscription) => unit;

  let remove: (state, RxCore.subscription) => unit;

  let clear: state => unit;
};

module MultipleAssignment: {
  include BooleanSubscription;

  let create: RxCore.subscription => (RxCore.subscription, state);

  let set: (state, RxCore.subscription) => unit;
};

module SingleAssignment: {
  include BooleanSubscription;

  let create: unit => (RxCore.subscription, state);

  let set: (state, RxCore.subscription) => unit;
};
