/* Internal module (see Rx.Observable) */

let empty: RxCore.observable('a);

let error: exn => RxCore.observable('a);

let never: RxCore.observable('a);

let return: 'a => RxCore.observable('a);

let materialize:
  RxCore.observable('a) => RxCore.observable(RxCore.notification('a));

let dematerialize:
  RxCore.observable(RxCore.notification('a)) => RxCore.observable('a);

let length: RxCore.observable('a) => RxCore.observable(int);

let drop: (int, RxCore.observable('a)) => RxCore.observable('a);

let take: (int, RxCore.observable('a)) => RxCore.observable('a);

let take_last: (int, RxCore.observable('a)) => RxCore.observable('a);

let single: RxCore.observable('a) => RxCore.observable('a);

let append:
  (RxCore.observable('a), RxCore.observable('a)) => RxCore.observable('a);

let merge:
  RxCore.observable(RxCore.observable('a)) => RxCore.observable('a);

let map: ('a => 'b, RxCore.observable('a)) => RxCore.observable('b);

let bind:
  (RxCore.observable('a), 'a => RxCore.observable('b)) =>
  RxCore.observable('b);

module Blocking: {
  let to_enum: RxCore.observable('a) => BatEnum.t('a);

  let single: RxCore.observable('a) => 'a;
};

module type Scheduled = {
  let subscribe_on_this: RxCore.observable('a) => RxCore.observable('a);

  let of_enum: BatEnum.t('a) => RxCore.observable('a);

  let interval: float => RxCore.observable(int);
};

module MakeScheduled: (Scheduler: RxScheduler.S) => Scheduled;

module CurrentThread: Scheduled;

module Immediate: Scheduled;

module NewThread: Scheduled;

module Lwt: Scheduled;

module Test: Scheduled;
