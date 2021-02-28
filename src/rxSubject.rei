let create: unit => (RxCore.subject('a), RxCore.subscription);

module Replay: {
  let create: unit => (RxCore.subject('a), RxCore.subscription);
};

module Behavior: {
  let create: 'a => (RxCore.subject('a), RxCore.subscription);
};

module Async: {let create: unit => (RxCore.subject('a), RxCore.subscription);
};
