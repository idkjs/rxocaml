open OUnit2;

let test_wait_graceful = _ => {
  let ok = ref(false);
  let async_lock = RxAsyncLock.create();
  RxAsyncLock.wait(async_lock, () => ok := true);
  assert_bool("ok should be true", ok^);
};

let test_wait_fail = _ => {
  let l = RxAsyncLock.create();
  let ex = Failure("test");
  try(
    {
      RxAsyncLock.wait(l, () => raise(ex));
      assert_failure("should raise an exception");
    }
  ) {
  | e => assert_equal(ex, e)
  };
  RxAsyncLock.wait(l, () => assert_failure("l has faulted; should not run"));
};

let test_wait_queues_work = _ => {
  let l = RxAsyncLock.create();
  let l1 = ref(false);
  let l2 = ref(false);
  RxAsyncLock.wait(
    l,
    () => {
      RxAsyncLock.wait(
        l,
        () => {
          assert_bool("l1 should be true", l1^);
          l2 := true;
        },
      );
      l1 := true;
    },
  );
  assert_bool("l2 should be true", l2^);
};

let test_dispose = _ => {
  let l = RxAsyncLock.create();
  let l1 = ref(false);
  let l2 = ref(false);
  let l3 = ref(false);
  let l4 = ref(false);
  RxAsyncLock.wait(
    l,
    () => {
      RxAsyncLock.wait(
        l,
        () => {
          RxAsyncLock.wait(l, () => l3 := true);
          l2 := true;
          RxAsyncLock.dispose(l);

          RxAsyncLock.wait(l, () => l4 := true);
        },
      );

      l1 := true;
    },
  );
  assert_bool("l1 should be true", l1^);
  assert_bool("l2 should be true", l2^);
  assert_equal(false, l3^);
  assert_equal(false, l4^);
};

let suite =
  "Async lock tests"
  >::: [
    "test_wait_graceful" >:: test_wait_graceful,
    "test_wait_fail" >:: test_wait_fail,
    "test_wait_queues_work" >:: test_wait_queues_work,
    "test_dispose" >:: test_dispose,
  ];
