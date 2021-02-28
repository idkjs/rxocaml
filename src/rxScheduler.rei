/* Internal module. (see Rx.Scheduler) */

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

module MakeScheduler: (BaseScheduler: Base) => S;

module CurrentThread: S;

module Immediate: S;

module NewThread: S;

module Lwt: S;

module Test: {
  include S;

  let now: unit => float;

  let trigger_actions: float => unit;

  let trigger_actions_until_now: unit => unit;

  let advance_time_to: float => unit;

  let advance_time_by: float => unit;
};
