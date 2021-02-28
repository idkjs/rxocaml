open OUnit2;

let test_unsafe_get = _ => {
  let v = RxAtomicData.create(0);
  assert_equal(0, RxAtomicData.unsafe_get(v));
};

let test_get = _ => {
  let v = RxAtomicData.create(0);
  assert_equal(0, RxAtomicData.get(v));
};

let test_set = _ => {
  let v = RxAtomicData.create(0);
  RxAtomicData.set(1, v);
  assert_equal(1, RxAtomicData.get(v));
};

let test_get_and_set = _ => {
  let v = RxAtomicData.create(0);
  let v' = RxAtomicData.get_and_set(1, v);
  assert_equal(1, RxAtomicData.get(v));
  assert_equal(0, v');
};

let test_update = _ => {
  let v = RxAtomicData.create(0);
  RxAtomicData.update(succ, v);
  assert_equal(1, RxAtomicData.get(v));
};

let test_update_and_get = _ => {
  let v = RxAtomicData.create(0);
  let v' = RxAtomicData.update_and_get(succ, v);
  assert_equal(1, v');
};

let test_compare_and_set = _ => {
  let v = RxAtomicData.create(0);
  let old_content = RxAtomicData.compare_and_set(0, 1, v);
  assert_equal(0, old_content);
  assert_equal(1, RxAtomicData.get(v));
};

let test_concurrent_update = _ => {
  let v = RxAtomicData.create(0);
  let count = 10;
  let start = Condition.create();
  let lock = Mutex.create();
  let f = () => {
    BatMutex.synchronize(~lock, () => Condition.wait(start, lock), ());
    RxAtomicData.update(
      value => {
        let value' = succ(value);
        Thread.delay(Random.float(0.1));
        value';
      },
      v,
    );
  };

  let threads =
    BatEnum.range(1, ~until=count)
    |> BatEnum.fold(
         (ts, _) => {
           let thread = Thread.create(f, ());
           [thread, ...ts];
         },
         [],
       );

  /* Wait for all the threads to start */
  Thread.delay(0.1);
  Condition.broadcast(start);
  List.iter(thread => Thread.join(thread), threads);
  assert_equal(count, RxAtomicData.get(v));
};

let suite =
  "Atomic data tests"
  >::: [
    "test_unsafe_get" >:: test_unsafe_get,
    "test_get" >:: test_get,
    "test_set" >:: test_set,
    "test_get_and_set" >:: test_get_and_set,
    "test_update" >:: test_update,
    "test_update_and_get" >:: test_update_and_get,
    "test_compare_and_set" >:: test_compare_and_set,
    "test_concurrent_update" >:: test_concurrent_update,
  ];
