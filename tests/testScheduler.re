open OUnit2;

let test_current_thread_schedule_action = _ => {
  let id = Utils.current_thread_id();
  let ran = ref(false);
  let _ =
    Rx.Scheduler.CurrentThread.schedule_absolute(() => {
      assert_equal(id, Utils.current_thread_id());
      ran := true;
      Rx.Subscription.empty;
    });
  assert_bool("ran should be true", ran^);
};

let test_current_thread_schedule_action_error = _ => {
  let ex = Failure("test");
  try({
    let _ = Rx.Scheduler.CurrentThread.schedule_absolute(() => raise(ex));
    assert_failure("Should raise an exception");
  }) {
  | e => assert_equal(ex, e)
  };
};

let test_current_thread_schedule_action_nested = _ => {
  let id = Utils.current_thread_id();
  let ran = ref(false);
  let _ =
    Rx.Scheduler.CurrentThread.schedule_absolute(() => {
      assert_equal(id, Utils.current_thread_id());
      Rx.Scheduler.CurrentThread.schedule_absolute(() => {
        ran := true;
        Rx.Subscription.empty;
      });
    });
  assert_bool("ran should be true", ran^);
};

let test_current_thread_schedule_relative_action_nested = _ => {
  let id = Utils.current_thread_id();
  let ran = ref(false);
  let _ =
    Rx.Scheduler.CurrentThread.schedule_absolute(() => {
      assert_equal(id, Utils.current_thread_id());
      Rx.Scheduler.CurrentThread.schedule_relative(
        0.1,
        () => {
          ran := true;
          Rx.Subscription.empty;
        },
      );
    });
  assert_bool("ran should be true", ran^);
};

let test_current_thread_schedule_relative_action_due = _ => {
  let id = Utils.current_thread_id();
  let ran = ref(false);
  let start = Unix.gettimeofday();
  let stop = ref(0.0);
  let _ =
    Rx.Scheduler.CurrentThread.schedule_relative(
      0.1,
      () => {
        stop := Unix.gettimeofday();
        assert_equal(id, Utils.current_thread_id());
        ran := true;
        Rx.Subscription.empty;
      },
    );
  assert_bool("ran should be true", ran^);
  let elapsed = stop^ -. start;
  assert_bool("Elapsed time should be > 80ms", elapsed > 0.08);
};

let test_current_thread_schedule_relative_action_due_nested = _ => {
  let ran = ref(false);
  let start = Unix.gettimeofday();
  let stop = ref(0.0);
  let _ =
    Rx.Scheduler.CurrentThread.schedule_relative(0.1, () =>
      Rx.Scheduler.CurrentThread.schedule_relative(
        0.1,
        () => {
          stop := Unix.gettimeofday();
          ran := true;
          Rx.Subscription.empty;
        },
      )
    );
  assert_bool("ran should be true", ran^);
  let elapsed = stop^ -. start;
  assert_bool("Elapsed time should be > 180ms", elapsed > 0.18);
};

let test_current_thread_cancel = _ => {
  let ran1 = ref(false);
  let ran2 = ref(false);
  let _ =
    Rx.Scheduler.CurrentThread.schedule_absolute(() =>
      RxScheduler.CurrentThread.schedule_absolute(() => {
        ran1 := true;
        let unsubscribe =
          Rx.Scheduler.CurrentThread.schedule_relative(
            1.0,
            () => {
              ran2 := true;
              Rx.Subscription.empty;
            },
          );

        unsubscribe();
        Rx.Subscription.empty;
      })
    );
  assert_equal(~msg="ran1", true, ran1^);
  assert_equal(~msg="ran2", false, ran2^);
};

let test_current_thread_schedule_nested_actions = _ => {
  let queue = Queue.create();
  let first_step_start = () => Queue.add("first_step_start", queue);
  let first_step_end = () => Queue.add("first_step_end", queue);
  let second_step_start = () => Queue.add("second_step_start", queue);
  let second_step_end = () => Queue.add("second_step_end", queue);
  let third_step_start = () => Queue.add("third_step_start", queue);
  let third_step_end = () => Queue.add("third_step_end", queue);
  let first_action = () => {
    first_step_start();
    first_step_end();
    Rx.Subscription.empty;
  };

  let second_action = () => {
    second_step_start();
    let s = Rx.Scheduler.CurrentThread.schedule_absolute(first_action);
    second_step_end();
    s;
  };

  let third_action = () => {
    third_step_start();
    let s = Rx.Scheduler.CurrentThread.schedule_absolute(second_action);
    third_step_end();
    s;
  };

  let _ = Rx.Scheduler.CurrentThread.schedule_absolute(third_action);
  let in_order = BatQueue.enum(queue) |> BatList.of_enum;
  assert_equal(
    ~printer=
      xs => BatPrintf.sprintf2("%a", BatList.print(BatString.print), xs),
    [
      "third_step_start",
      "third_step_end",
      "second_step_start",
      "second_step_end",
      "first_step_start",
      "first_step_end",
    ],
    in_order,
  );
};

let test_current_thread_schedule_recursion = _ => {
  let counter = ref(0);
  let count = 10;
  let _ =
    Rx.Scheduler.CurrentThread.schedule_recursive(self => {
      incr(counter);
      if (counter^ < count) {
        self();
      } else {
        Rx.Subscription.empty;
      };
    });

  assert_equal(count, counter^);
};

let test_immediate_schedule_action = _ => {
  let id = Utils.current_thread_id();
  let ran = ref(false);
  let _ =
    Rx.Scheduler.Immediate.schedule_absolute(() => {
      assert_equal(id, Utils.current_thread_id());
      ran := true;
      Rx.Subscription.empty;
    });
  assert_bool("ran should be true", ran^);
};

let test_immediate_schedule_nested_actions = _ => {
  let queue = Queue.create();
  let first_step_start = () => Queue.add("first_step_start", queue);
  let first_step_end = () => Queue.add("first_step_end", queue);
  let second_step_start = () => Queue.add("second_step_start", queue);
  let second_step_end = () => Queue.add("second_step_end", queue);
  let third_step_start = () => Queue.add("third_step_start", queue);
  let third_step_end = () => Queue.add("third_step_end", queue);
  let first_action = () => {
    first_step_start();
    first_step_end();
    Rx.Subscription.empty;
  };

  let second_action = () => {
    second_step_start();
    let s = Rx.Scheduler.Immediate.schedule_absolute(first_action);
    second_step_end();
    s;
  };

  let third_action = () => {
    third_step_start();
    let s = Rx.Scheduler.Immediate.schedule_absolute(second_action);
    third_step_end();
    s;
  };

  let _ = Rx.Scheduler.Immediate.schedule_absolute(third_action);
  let in_order = BatQueue.enum(queue) |> BatList.of_enum;
  assert_equal(
    ~printer=
      xs => BatPrintf.sprintf2("%a", BatList.print(BatString.print), xs),
    [
      "third_step_start",
      "second_step_start",
      "first_step_start",
      "first_step_end",
      "second_step_end",
      "third_step_end",
    ],
    in_order,
  );
};

let test_new_thread_schedule_action = _ => {
  let id = Utils.current_thread_id();
  let ran = ref(false);
  let _ =
    Rx.Scheduler.NewThread.schedule_absolute(() => {
      assert_bool(
        "New thread scheduler should create schedule work on a different thread",
        id != Utils.current_thread_id(),
      );
      ran := true;
      Rx.Subscription.empty;
    });
  /* Wait for the other thread to run */
  Thread.delay(0.1);
  assert_bool("ran should be true", ran^);
};

let test_new_thread_cancel_action = _ => {
  let unsubscribe =
    Rx.Scheduler.NewThread.schedule_absolute(() =>
      assert_failure("This action should not run")
    );
  unsubscribe();
  /* Wait for the other thread */
  Thread.delay(0.1);
};

let test_lwt_schedule_action = _ => {
  let ran = ref(false);
  let _ =
    Rx.Scheduler.Lwt.schedule_absolute(() => {
      ran := true;
      Rx.Subscription.empty;
    });
  assert_bool("ran should be true", ran^);
};

let test_lwt_cancel_action = _ => {
  let unsubscribe =
    Rx.Scheduler.Lwt.schedule_absolute(() =>
      assert_failure("This action should not run")
    );
  unsubscribe();
};

let test_lwt_schedule_periodically = _ => {
  let counter = ref(0);
  let unsubscribe =
    Rx.Scheduler.Lwt.schedule_periodically(
      0.1,
      () => {
        incr(counter);
        Rx.Subscription.empty;
      },
    );
  let sleep = Lwt_unix.sleep(0.15);
  let () = Lwt_main.run(sleep);
  unsubscribe();
  assert_equal(~printer=string_of_int, 2, counter^);
};

let suite =
  "Scheduler tests"
  >::: [
    "test_current_thread_schedule_action"
    >:: test_current_thread_schedule_action,
    "test_current_thread_schedule_action_error"
    >:: test_current_thread_schedule_action_error,
    "test_current_thread_schedule_action_nested"
    >:: test_current_thread_schedule_action_nested,
    "test_current_thread_schedule_relative_action_nested"
    >:: test_current_thread_schedule_relative_action_nested,
    "test_current_thread_schedule_relative_action_due"
    >:: test_current_thread_schedule_relative_action_due,
    "test_current_thread_schedule_relative_action_due_nested"
    >:: test_current_thread_schedule_relative_action_due_nested,
    "test_current_thread_cancel" >:: test_current_thread_cancel,
    "test_current_thread_schedule_nested_actions"
    >:: test_current_thread_schedule_nested_actions,
    "test_current_thread_schedule_recursion"
    >:: test_current_thread_schedule_recursion,
    "test_immediate_schedule_action" >:: test_immediate_schedule_action,
    "test_immediate_schedule_nested_actions"
    >:: test_immediate_schedule_nested_actions,
    "test_new_thread_schedule_action" >:: test_new_thread_schedule_action,
    "test_new_thread_cancel_action" >:: test_new_thread_cancel_action,
    "test_lwt_schedule_action" >:: test_lwt_schedule_action,
    "test_lwt_cancel_action" >:: test_lwt_cancel_action,
    "test_lwt_schedule_periodically" >:: test_lwt_schedule_periodically,
  ];
