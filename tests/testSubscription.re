open OUnit2;

let test_unsubscribe_only_once = _ => {
  let counter = ref(0);
  let unsubscribe = Rx.Subscription.create(() => incr(counter));
  unsubscribe();
  unsubscribe();
  assert_equal(1, counter^);
};

let test_boolean_subscription = _ => {
  let counter = ref(0);
  let (boolean_subscription, state) =
    Rx.Subscription.Boolean.create(() => incr(counter));
  assert_equal(false, Rx.Subscription.Boolean.is_unsubscribed(state));
  boolean_subscription();
  assert_equal(true, Rx.Subscription.Boolean.is_unsubscribed(state));
  boolean_subscription();
  assert_equal(true, Rx.Subscription.Boolean.is_unsubscribed(state));
  assert_equal(1, counter^);
};

let test_composite_subscription_success = _ => {
  let counter = ref(0);
  let (composite_subscription, state) = Rx.Subscription.Composite.create([]);
  Rx.Subscription.Composite.add(state, () => incr(counter));
  Rx.Subscription.Composite.add(state, () => incr(counter));
  composite_subscription();
  assert_equal(2, counter^);
};

let test_composite_subscription_unsubscribe_all = _ => {
  let counter = ref(0);
  let (composite_subscription, state) = Rx.Subscription.Composite.create([]);
  let count = 10;
  let start = Condition.create();
  let mutex = Mutex.create();
  BatEnum.range(1, ~until=count)
  |> BatEnum.iter(_ =>
       Rx.Subscription.Composite.add(state, () => incr(counter))
     );
  let threads =
    BatEnum.range(1, ~until=count)
    |> BatEnum.fold(
         (ts, _) => {
           let thread =
             Thread.create(
               () =>
                 BatMutex.synchronize(
                   ~lock=mutex,
                   () => {
                     Condition.wait(start, mutex);
                     composite_subscription();
                   },
                   (),
                 ),
               (),
             );

           [thread, ...ts];
         },
         [],
       );

  /* Wait for all the threads to start */
  Thread.delay(0.1);
  Condition.broadcast(start);
  List.iter(thread => Thread.join(thread), threads);
  assert_equal(count, counter^);
};

let test_composite_subscription_exception = _ => {
  let counter = ref(0);
  let ex = Failure("failed on first one");
  let (composite_subscription, state) = Rx.Subscription.Composite.create([]);
  Rx.Subscription.Composite.add(state, () => raise(ex));
  Rx.Subscription.Composite.add(state, () => incr(counter));
  try(
    {
      composite_subscription();
      assert_failure("Expecting an exception");
    }
  ) {
  | Rx.Subscription.Composite.CompositeException(es) =>
    assert_equal(ex, List.hd(es))
  };
  /* we should still have unsubscribed to the second one */
  assert_equal(1, counter^);
};

let test_composite_subscription_remove = _ => {
  let (s1, state1) = Rx.Subscription.Boolean.create(() => ());
  let (s2, state2) = Rx.Subscription.Boolean.create(() => ());
  let (s, state) = Rx.Subscription.Composite.create([]);
  Rx.Subscription.Composite.add(state, s1);
  Rx.Subscription.Composite.add(state, s2);
  Rx.Subscription.Composite.remove(state, s1);
  assert_equal(true, Rx.Subscription.Boolean.is_unsubscribed(state1));
  assert_equal(false, Rx.Subscription.Boolean.is_unsubscribed(state2));
};

let test_composite_subscription_clear = _ => {
  let (s1, state1) = Rx.Subscription.Boolean.create(() => ());
  let (s2, state2) = Rx.Subscription.Boolean.create(() => ());
  let (s, state) = Rx.Subscription.Composite.create([]);
  Rx.Subscription.Composite.add(state, s1);
  Rx.Subscription.Composite.add(state, s2);
  assert_equal(false, Rx.Subscription.Boolean.is_unsubscribed(state1));
  assert_equal(false, Rx.Subscription.Boolean.is_unsubscribed(state2));
  Rx.Subscription.Composite.clear(state);
  assert_equal(true, Rx.Subscription.Boolean.is_unsubscribed(state1));
  assert_equal(true, Rx.Subscription.Boolean.is_unsubscribed(state2));
  assert_equal(false, Rx.Subscription.Composite.is_unsubscribed(state));
  let (s3, state3) = Rx.Subscription.Boolean.create(() => ());
  Rx.Subscription.Composite.add(state, s3);
  s();
  assert_equal(true, Rx.Subscription.Boolean.is_unsubscribed(state3));
  assert_equal(true, Rx.Subscription.Composite.is_unsubscribed(state));
};

let test_composite_subscription_unsubscribe_idempotence = _ => {
  let counter = ref(0);
  let (s, state) = Rx.Subscription.Composite.create([]);
  Rx.Subscription.Composite.add(state, () => incr(counter));
  s();
  s();
  s();
  /* We should have only unsubscribed once */
  assert_equal(1, counter^);
};

let test_composite_subscription_unsubscribe_idempotence_concurrently = _ => {
  let counter = ref(0);
  let (s, state) = Rx.Subscription.Composite.create([]);
  let count = 10;
  let start = Condition.create();
  let mutex = Mutex.create();
  Rx.Subscription.Composite.add(state, () => incr(counter));
  let threads =
    BatEnum.range(1, ~until=count)
    |> BatEnum.fold(
         (ts, _) => {
           let thread =
             Thread.create(
               () =>
                 BatMutex.synchronize(
                   ~lock=mutex,
                   () => {
                     Condition.wait(start, mutex);
                     s();
                   },
                   (),
                 ),
               (),
             );

           [thread, ...ts];
         },
         [],
       );

  /* Wait for all the threads to start */
  Thread.delay(0.1);
  Condition.broadcast(start);
  List.iter(thread => Thread.join(thread), threads);
  /* We should have only unsubscribed once */
  assert_equal(1, counter^);
};

let test_multiple_assignment_subscription = _ => {
  let (m, state) =
    Rx.Subscription.MultipleAssignment.create(Rx.Subscription.empty);
  let unsub1 = ref(false);
  let s1 = Rx.Subscription.create(() => unsub1 := true);
  Rx.Subscription.MultipleAssignment.set(state, s1);
  assert_equal(
    false,
    Rx.Subscription.MultipleAssignment.is_unsubscribed(state),
  );
  let unsub2 = ref(false);
  let s2 = Rx.Subscription.create(() => unsub2 := true);
  Rx.Subscription.MultipleAssignment.set(state, s2);
  assert_equal(
    false,
    Rx.Subscription.MultipleAssignment.is_unsubscribed(state),
  );
  assert_equal(false, unsub1^);
  m();
  assert_equal(true, unsub2^);
  assert_equal(
    true,
    Rx.Subscription.MultipleAssignment.is_unsubscribed(state),
  );
  let unsub3 = ref(false);
  let s3 = Rx.Subscription.create(() => unsub3 := true);
  Rx.Subscription.MultipleAssignment.set(state, s3);
  assert_equal(true, unsub3^);
  assert_equal(
    true,
    Rx.Subscription.MultipleAssignment.is_unsubscribed(state),
  );
};

let test_single_assignment_subscription = _ => {
  let (m, state) = Rx.Subscription.SingleAssignment.create();
  let unsub1 = ref(false);
  let s1 = Rx.Subscription.create(() => unsub1 := true);
  Rx.Subscription.SingleAssignment.set(state, s1);
  assert_equal(
    false,
    Rx.Subscription.SingleAssignment.is_unsubscribed(state),
  );
  let unsub2 = ref(false);
  let s2 = Rx.Subscription.create(() => unsub2 := true);
  try(
    {
      Rx.Subscription.SingleAssignment.set(state, s2);
      assert_failure("Should raise an exception");
    }
  ) {
  | e => assert_equal(Failure("SingleAssignment"), e)
  };
  assert_equal(
    false,
    Rx.Subscription.SingleAssignment.is_unsubscribed(state),
  );
  assert_equal(false, unsub1^);
  m();
  assert_equal(true, unsub1^);
  assert_equal(false, unsub2^);
  assert_equal(
    true,
    Rx.Subscription.SingleAssignment.is_unsubscribed(state),
  );
};

let suite =
  "Subscription tests"
  >::: [
    "test_unsubscribe_only_once" >:: test_unsubscribe_only_once,
    "test_boolean_subscription" >:: test_boolean_subscription,
    "test_composite_subscription_success"
    >:: test_composite_subscription_success,
    "test_composite_subscription_unsubscribe_all"
    >:: test_composite_subscription_unsubscribe_all,
    "test_composite_subscription_exception"
    >:: test_composite_subscription_exception,
    "test_composite_subscription_remove" >:: test_composite_subscription_remove,
    "test_composite_subscription_clear" >:: test_composite_subscription_clear,
    "test_composite_subscription_unsubscribe_idempotence"
    >:: test_composite_subscription_unsubscribe_idempotence,
    "test_composite_subscription_unsubscribe_idempotence_concurrently"
    >:: test_composite_subscription_unsubscribe_idempotence_concurrently,
    "test_multiple_assignment_subscription"
    >:: test_multiple_assignment_subscription,
    "test_single_assignment_subscription"
    >:: test_single_assignment_subscription,
  ];
