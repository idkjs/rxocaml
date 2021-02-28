open OUnit2;

let test_create_subject = _ => {
  let c1 = ref(0);
  let c2 = ref(0);
  let (subject, _) = Rx.Subject.create();
  let ((_, _, on_next), observable) = subject;
  let observer1 = Rx.Observer.create(_ => incr(c1));
  let unsubscribe1 = observable(observer1);
  let observer2 = Rx.Observer.create(_ => incr(c2));
  let unsubscribe2 = observable(observer2);
  on_next(1);
  on_next(2);
  assert_equal(2, c1^);
  assert_equal(2, c2^);
  unsubscribe2();
  on_next(3);
  assert_equal(3, c1^);
  assert_equal(2, c2^);
  unsubscribe1();
  on_next(4);
  assert_equal(3, c1^);
  assert_equal(2, c2^);
};

let test_subject_unsubscribe = _ => {
  let c1 = ref(0);
  let c2 = ref(0);
  let (subject, unsubscribe) = Rx.Subject.create();
  let ((_, _, on_next), observable) = subject;
  let observer1 = Rx.Observer.create(_ => incr(c1));
  let _ = observable(observer1)|>ignore;
  let observer2 = Rx.Observer.create(_ => incr(c2));
  let _ = observable(observer2)|>ignore;
  on_next(1);
  on_next(2);
  assert_equal(2, c1^);
  assert_equal(2, c2^);
  unsubscribe();
  on_next(3);
  assert_equal(2, c1^);
  assert_equal(2, c2^);
};

let test_subject_on_completed = _ => {
  let on_completed1 = ref(false);
  let on_completed2 = ref(false);
  let (subject, _) = Rx.Subject.create();
  let ((on_completed, _, _), observable) = subject;
  let observer1 =
    Rx.Observer.create(~on_completed=() => on_completed1 := true, _ => ());
  let _ = observable(observer1)|>ignore;
  let observer2 =
    Rx.Observer.create(~on_completed=() => on_completed2 := true, _ => ());
  let _ = observable(observer2)|>ignore;
  on_completed();
  assert_equal(true, on_completed1^);
  assert_equal(true, on_completed2^);
};

let test_subject_on_error = _ => {
  let on_error1 = ref(false);
  let on_error2 = ref(false);
  let (subject, _) = Rx.Subject.create();
  let ((_, on_error, _), observable) = subject;
  let observer1 =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(Failure("test"), e);
          on_error1 := true;
        },
      _ => (),
    );
  let _ = observable(observer1)|>ignore;
  let observer2 =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(Failure("test"), e);
          on_error2 := true;
        },
      _ => (),
    );
  let _ = observable(observer2)|>ignore;
  on_error(Failure("test"));
  assert_equal(true, on_error1^);
  assert_equal(true, on_error2^);
};

let test_create_replay_subject = _ => {
  let c1 = ref(0);
  let c2 = ref(0);
  let (subject, unsubscribe) = Rx.Subject.Replay.create();
  let ((_, _, on_next), observable) = subject;
  on_next(1);
  on_next(2);
  let observer1 = Rx.Observer.create(_ => incr(c1));
  let _ = observable(observer1)|>ignore;
  let observer2 = Rx.Observer.create(_ => incr(c2));
  let _ = observable(observer2)|>ignore;
  assert_equal(2, c1^);
  assert_equal(2, c2^);
  on_next(3);
  assert_equal(3, c1^);
  assert_equal(3, c2^);
  unsubscribe();
  on_next(4);
  assert_equal(3, c1^);
  assert_equal(3, c2^);
};

let test_replay_subject_on_completed = _ => {
  let on_completed1 = ref(false);
  let on_completed2 = ref(false);
  let (subject, _) = Rx.Subject.Replay.create();
  let ((on_completed, _, on_next), observable) = subject;
  on_completed();
  let observer1 =
    Rx.Observer.create(
      ~on_completed=() => on_completed1 := true,
      _ => assert_failure("on_next should not be called"),
    );
  let _ = observable(observer1)|>ignore;
  let observer2 =
    Rx.Observer.create(
      ~on_completed=() => on_completed2 := true,
      _ => assert_failure("on_next should not be called"),
    );
  let _ = observable(observer2)|>ignore;
  assert_equal(true, on_completed1^);
  assert_equal(true, on_completed2^);
  on_next(1);
};

let test_replay_subject_on_error = _ => {
  let on_error1 = ref(false);
  let on_error2 = ref(false);
  let (subject, _) = Rx.Subject.Replay.create();
  let ((_, on_error, on_next), observable) = subject;
  on_error(Failure("test"));
  let observer1 =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(Failure("test"), e);
          on_error1 := true;
        },
      _ => assert_failure("on_next should not be called"),
    );
  let _ = observable(observer1)|>ignore;
  let observer2 =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(Failure("test"), e);
          on_error2 := true;
        },
      _ => assert_failure("on_next should not be called"),
    );
  let _ = observable(observer2)|>ignore;
  assert_equal(true, on_error1^);
  assert_equal(true, on_error2^);
  on_next(1);
};

let test_create_behavior_subject = _ => {
  let v1 = ref("");
  let v2 = ref("");
  let (subject, unsubscribe) = Rx.Subject.Behavior.create("default");
  let ((_, _, on_next), observable) = subject;
  let observer1 = Rx.Observer.create(v => v1 := v);
  let _ = observable(observer1)|>ignore;
  assert_equal("default", v1^);
  assert_equal("", v2^);
  on_next("one");
  assert_equal("one", v1^);
  assert_equal("", v2^);
  on_next("two");
  let observer2 = Rx.Observer.create(v => v2 := v);
  let _ = observable(observer2)|>ignore;
  assert_equal("two", v1^);
  assert_equal("two", v2^);
  unsubscribe();
  on_next("three");
  assert_equal("two", v1^);
  assert_equal("two", v2^);
};

let test_behavior_subject_on_completed = _ => {
  let completed = ref(false);
  let (subject, _) = Rx.Subject.Behavior.create(0);
  let ((on_completed, _, on_next), observable) = subject;
  on_next(1);
  on_completed();
  let observer =
    Rx.Observer.create(
      ~on_completed=() => completed := true,
      _ => assert_failure("on_next should not be called"),
    );
  let _ = observable(observer)|>ignore;
  assert_equal(true, completed^);
};

let test_behavior_subject_on_error = _ => {
  let error = ref(false);
  let (subject, _) = Rx.Subject.Behavior.create(0);
  let ((_, on_error, on_next), observable) = subject;
  on_next(1);
  on_error(Failure("test"));
  let observer =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(Failure("test"), e);
          error := true;
        },
      _ => assert_failure("on_next should not be called"),
    );
  let _ = observable(observer)|>ignore;
  assert_equal(true, error^);
};

let test_create_async_subject = _ => {
  let v1 = ref("");
  let v2 = ref("");
  let (subject, _) = Rx.Subject.Async.create();
  let ((on_completed, _, on_next), observable) = subject;
  let observer1 = Rx.Observer.create(v => v1 := v);
  let _ = observable(observer1)|>ignore;
  assert_equal("", v1^);
  assert_equal("", v2^);
  on_next("one");
  assert_equal("", v1^);
  assert_equal("", v2^);
  on_next("two");
  on_completed();
  assert_equal("two", v1^);
  assert_equal("", v2^);
  let observer2 = Rx.Observer.create(v => v2 := v);
  let _ = observable(observer2)|>ignore;
  assert_equal("two", v1^);
  assert_equal("two", v2^);
};

let test_async_subject_on_error = _ => {
  let error = ref(false);
  let (subject, _) = Rx.Subject.Async.create();
  let ((_, on_error, on_next), observable) = subject;
  let observer =
    Rx.Observer.create(
      ~on_error=
        e => {
          assert_equal(Failure("test"), e);
          error := true;
        },
      _ => assert_failure("on_next should not be called"),
    );
  observable(observer)|>ignore;
  on_next(1);
  on_error(Failure("test"));
  assert_equal(true, error^);
};

let suite =
  "Subjects tests"
  >::: [
    "test_create_subject" >:: test_create_subject,
    "test_subject_unsubscribe" >:: test_subject_unsubscribe,
    "test_subject_on_completed" >:: test_subject_on_completed,
    "test_subject_on_error" >:: test_subject_on_error,
    "test_create_replay_subject" >:: test_create_replay_subject,
    "test_replay_subject_on_completed" >:: test_replay_subject_on_completed,
    "test_replay_subject_on_error" >:: test_replay_subject_on_error,
    "test_create_behavior_subject" >:: test_create_behavior_subject,
    "test_behavior_subject_on_completed" >:: test_behavior_subject_on_completed,
    "test_behavior_subject_on_error" >:: test_behavior_subject_on_error,
    "test_create_async_subject" >:: test_create_async_subject,
    "test_async_subject_on_error" >:: test_async_subject_on_error,
  ];
