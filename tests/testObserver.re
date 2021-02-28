open OUnit2;

let test_create_on_next = _ => {
  let next = ref(false);
  let observer: RxCore.observer(int) = (
    Rx.Observer.create(x => {
      assert_equal(42, x);
      next := true;
    }):
      RxCore.observer(int)
  );
  let (on_completed, _, on_next) = observer;
  on_next(42);
  assert_equal(true, next^);
  on_completed();
};

let test_create_on_next_has_errors = _ => {
  let failure = Failure("error");
  let next = ref(false);
  let observer: RxCore.observer(int) = (
    Rx.Observer.create(x => {
      assert_equal(42, x);
      next := true;
    }):
      RxCore.observer(int)
  );
  let (_, on_error, on_next) = observer;
  on_next(42);
  assert_equal(true, next^);
  try(
    {
      on_error(failure);
      assert_failure("Should raise the exception");
    }
  ) {
  | e => assert_equal(failure, e)
  };
};

let test_create_on_next_on_completed = _ => {
  let next = ref(false);
  let completed = ref(false);
  let observer =
    Rx.Observer.create(
      ~on_completed=() => completed := true,
      x => {
        assert_equal(42, x);
        next := true;
      },
    );
  let (on_completed, _, on_next) = observer;
  on_next(42);
  assert_equal(true, next^);
  assert_equal(false, completed^);
  on_completed();
  assert_equal(true, completed^);
};

let test_create_on_next_on_completed_has_errors = _ => {
  let failure = Failure("error");
  let next = ref(false);
  let completed = ref(false);
  let observer =
    Rx.Observer.create(
      ~on_completed=() => completed := true,
      x => {
        assert_equal(42, x);
        next := true;
      },
    );
  let (_, on_error, on_next) = observer;
  on_next(42);
  assert_equal(true, next^);
  assert_equal(false, completed^);
  try(
    {
      on_error(failure);
      assert_failure("Should raise the exception");
    }
  ) {
  | e => assert_equal(failure, e)
  };
  assert_equal(false, completed^);
};

let test_create_on_next_on_error = _ => {
  let failure = Failure("error");
  let next = ref(false);
  let error = ref(false);
  let observer =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(failure, e);
          error := true;
        },
      x => {
        assert_equal(42, x);
        next := true;
      },
    );
  let (_, on_error, on_next) = observer;
  on_next(42);
  assert_equal(true, next^);
  assert_equal(false, error^);
  on_error(failure);
  assert_equal(true, error^);
};

let test_create_on_next_on_error_hit_completed = _ => {
  let failure = Failure("error");
  let next = ref(false);
  let error = ref(false);
  let observer =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(failure, e);
          error := true;
        },
      x => {
        assert_equal(42, x);
        next := true;
      },
    );
  let (on_completed, _, on_next) = observer;
  on_next(42);
  assert_equal(true, next^);
  assert_equal(false, error^);
  on_completed();
  assert_equal(false, error^);
};

let test_create_on_next_on_error_on_completed_1 = _ => {
  let failure = Failure("error");
  let next = ref(false);
  let error = ref(false);
  let completed = ref(false);
  let observer =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(failure, e);
          error := true;
        },
      ~on_completed=() => completed := true,
      x => {
        assert_equal(42, x);
        next := true;
      },
    );
  let (on_completed, _, on_next) = observer;
  on_next(42);
  assert_equal(true, next^);
  assert_equal(false, error^);
  assert_equal(false, completed^);
  on_completed();
  assert_equal(true, completed^);
  assert_equal(false, error^);
};

let test_create_on_next_on_error_on_completed_2 = _ => {
  let failure = Failure("error");
  let next = ref(false);
  let error = ref(false);
  let completed = ref(false);
  let observer =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(failure, e);
          error := true;
        },
      ~on_completed=() => completed := true,
      x => {
        assert_equal(42, x);
        next := true;
      },
    );
  let (_, on_error, on_next) = observer;
  on_next(42);
  assert_equal(true, next^);
  assert_equal(false, error^);
  assert_equal(false, completed^);
  on_error(failure);
  assert_equal(true, error^);
  assert_equal(false, completed^);
};

let test_checked_observer_already_terminated_completed = _ => {
  let m = ref(0);
  let n = ref(0);
  let observer =
    Rx.Observer.create(
      ~on_error=_ => assert_failure("Should not call on_error"),
      ~on_completed=() => incr(n),
      _ => incr(m),
    )
    |> Rx.Observer.checked;
  let (on_completed, on_error, on_next) = observer;
  on_next(1);
  on_next(2);
  on_completed();
  assert_raises(Failure("Observer has already terminated."), () =>
    on_completed()
  );
  assert_raises(Failure("Observer has already terminated."), () =>
    on_error(Failure("test"))
  );
  assert_equal(2, m^);
  assert_equal(1, n^);
};

let test_checked_observer_already_terminated_error = _ => {
  let m = ref(0);
  let n = ref(0);
  let observer =
    Rx.Observer.create(
      ~on_error=_ => incr(n),
      ~on_completed=() => assert_failure("Should not call on_completed"),
      _ => incr(m),
    )
    |> Rx.Observer.checked;
  let (on_completed, on_error, on_next) = observer;
  on_next(1);
  on_next(2);
  on_error(Failure("test"));
  assert_raises(Failure("Observer has already terminated."), () =>
    on_completed()
  );
  assert_raises(Failure("Observer has already terminated."), () =>
    on_error(Failure("test2"))
  );
  assert_equal(2, m^);
  assert_equal(1, n^);
};

let test_checked_observer_reentrant_next = _ => {
  let n = ref(0);
  let reentrant_thunk = ref(() => ());
  let observer =
    Rx.Observer.create(
      ~on_error=_ => assert_failure("Should not call on_error"),
      ~on_completed=() => assert_failure("Should not call on_completed"),
      _ => {
        incr(n);
        reentrant_thunk^();
      },
    )
    |> Rx.Observer.checked;
  let (on_completed, on_error, on_next) = observer;
  reentrant_thunk :=
    (
      () => {
        assert_raises(
          ~msg="on_next", Failure("Reentrancy has been detected."), () =>
          on_next(9)
        );
        assert_raises(
          ~msg="on_error", Failure("Reentrancy has been detected."), () =>
          on_error(Failure("test"))
        );
        assert_raises(
          ~msg="on_completed", Failure("Reentrancy has been detected."), () =>
          on_completed()
        );
      }
    );
  on_next(1);
  assert_equal(1, n^);
};

let test_checked_observer_reentrant_error = _ => {
  let n = ref(0);
  let reentrant_thunk = ref(() => ());
  let observer =
    Rx.Observer.create(
      ~on_error=
        _ => {
          incr(n);
          reentrant_thunk^();
        },
      ~on_completed=() => assert_failure("Should not call on_completed"),
      _ => assert_failure("Should not call on_next"),
    )
    |> Rx.Observer.checked;
  let (on_completed, on_error, on_next) = observer;
  reentrant_thunk :=
    (
      () => {
        assert_raises(
          ~msg="on_next", Failure("Reentrancy has been detected."), () =>
          on_next(9)
        );
        assert_raises(
          ~msg="on_error", Failure("Reentrancy has been detected."), () =>
          on_error(Failure("test"))
        );
        assert_raises(
          ~msg="on_completed", Failure("Reentrancy has been detected."), () =>
          on_completed()
        );
      }
    );
  on_error(Failure("test"));
  assert_equal(1, n^);
};

let test_checked_observer_reentrant_completed = _ => {
  let n = ref(0);
  let reentrant_thunk = ref(() => ());
  let observer =
    Rx.Observer.create(
      ~on_error=_ => assert_failure("Should not call on_error"),
      ~on_completed=
        () => {
          incr(n);
          reentrant_thunk^();
        },
      _ => assert_failure("Should not call on_next"),
    )
    |> Rx.Observer.checked;
  let (on_completed, on_error, on_next) = observer;
  reentrant_thunk :=
    (
      () => {
        assert_raises(
          ~msg="on_next", Failure("Reentrancy has been detected."), () =>
          on_next(9)
        );
        assert_raises(
          ~msg="on_error", Failure("Reentrancy has been detected."), () =>
          on_error(Failure("test"))
        );
        assert_raises(
          ~msg="on_completed", Failure("Reentrancy has been detected."), () =>
          on_completed()
        );
      }
    );
  on_completed();
  assert_equal(1, n^);
};

let test_observer_synchronize_monitor_reentrant = _ => {
  let res = ref(false);
  let inOne = ref(false);
  let on_next_ref: ref(int => unit) = (ref(x => ()): ref(int => unit));
  let o =
    Rx.Observer.create(x =>
      if (x == 1) {
        inOne := true;
        on_next_ref^(2);
        inOne := false;
      } else if (x == 2) {
        res := inOne^;
      }
    );
  let (_, _, on_next) = Rx.Observer.synchronize(o);
  on_next_ref := on_next;
  on_next(1);
  assert_bool("res should be true", res^);
};

let test_observer_synchronize_async_lock_non_reentrant = _ => {
  let res = ref(false);
  let inOne = ref(false);
  let on_next_ref: ref(int => unit) = (ref(x => ()): ref(int => unit));
  let o =
    Rx.Observer.create(x =>
      if (x == 1) {
        inOne := true;
        on_next_ref^(2);
        inOne := false;
      } else if (x == 2) {
        res := ! inOne^;
      }
    );
  let (_, _, on_next) = Rx.Observer.synchronize_async_lock(o);
  on_next_ref := on_next;
  on_next(1);
  assert_bool("res should be true", res^);
};

let test_observer_synchronize_async_lock_on_next = _ => {
  let res = ref(false);
  let o =
    Rx.Observer.create(
      ~on_error=_ => assert_failure("Should not call on_error"),
      ~on_completed=() => assert_failure("Should not call on_completed"),
      x => res := x == 1,
    );
  let (_, _, on_next) = Rx.Observer.synchronize_async_lock(o);
  on_next(1);
  assert_bool("res should be true", res^);
};

let test_observer_synchronize_async_lock_on_error = _ => {
  let res = ref(Failure(""));
  let err = Failure("test");
  let o =
    Rx.Observer.create(
      ~on_error=e => res := e,
      ~on_completed=() => assert_failure("Should not call on_completed"),
      _ => assert_failure("Should not call on_next"),
    );
  let (_, on_error, _) = Rx.Observer.synchronize_async_lock(o);
  on_error(err);
  assert_equal(err, res^);
};

let test_observer_synchronize_async_lock_on_completed = _ => {
  let res = ref(false);
  let o =
    Rx.Observer.create(
      ~on_error=_ => assert_failure("Should not call on_error"),
      ~on_completed=() => res := true,
      _ => assert_failure("Should not call on_next"),
    );
  let (on_completed, _, _) = Rx.Observer.synchronize_async_lock(o);
  on_completed();
  assert_bool("res should be true", res^);
};

let suite =
  "Observer tests"
  >::: [
    "test_create_on_next" >:: test_create_on_next,
    "test_create_on_next_has_errors" >:: test_create_on_next_has_errors,
    "test_create_on_next_on_completed" >:: test_create_on_next_on_completed,
    "test_create_on_next_on_completed_has_errors"
    >:: test_create_on_next_on_completed_has_errors,
    "test_create_on_next_on_error" >:: test_create_on_next_on_error,
    "test_create_on_next_on_error_hit_completed"
    >:: test_create_on_next_on_error_hit_completed,
    "test_create_on_next_on_error_on_completed_1"
    >:: test_create_on_next_on_error_on_completed_1,
    "test_create_on_next_on_error_on_completed_2"
    >:: test_create_on_next_on_error_on_completed_2,
    "test_checked_observer_already_terminated_completed"
    >:: test_checked_observer_already_terminated_completed,
    "test_checked_observer_already_terminated_error"
    >:: test_checked_observer_already_terminated_error,
    "test_checked_observer_reentrant_next"
    >:: test_checked_observer_reentrant_next,
    "test_checked_observer_reentrant_error"
    >:: test_checked_observer_reentrant_error,
    "test_checked_observer_reentrant_completed"
    >:: test_checked_observer_reentrant_completed,
    "test_observer_synchronize_monitor_reentrant"
    >:: test_observer_synchronize_monitor_reentrant,
    "test_observer_synchronize_async_lock_non_reentrant"
    >:: test_observer_synchronize_async_lock_non_reentrant,
    "test_observer_synchronize_async_lock_on_next"
    >:: test_observer_synchronize_async_lock_on_next,
    "test_observer_synchronize_async_lock_on_error"
    >:: test_observer_synchronize_async_lock_on_error,
    "test_observer_synchronize_async_lock_on_completed"
    >:: test_observer_synchronize_async_lock_on_completed,
  ];
