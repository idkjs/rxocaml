module Observer: {
  type state('a);

  let create: unit => (RxCore.observer('a), state('a));

  let is_completed: state('a) => bool;

  let is_on_error: state('a) => bool;

  let get_error: state('a) => option(exn);

  let on_next_values: state('a) => list('a);
};
