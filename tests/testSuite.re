open OUnit2;

let tests = [
  TestObserver.suite,
  TestAsyncLock.suite,
  TestSubscription.suite,
  TestScheduler.suite,
  TestObservable.suite,
  TestAtomicData.suite,
  TestSubject.suite,
];

let build_suite = () => "RxOCaml test suite" >::: tests;

let _ = {
  let ounit_specs = [
    ("-verbose", Arg.Unit(_ => ()), "See oUnit doc"),
    ("-only-test", Arg.String(_ => ()), "See oUnit doc"),
    ("-list-test", Arg.String(_ => ()), "See oUnit doc"),
  ];
  let _ =
    Arg.parse(
      ounit_specs,
      _ => (),
      "Usage: " ++ Sys.argv[0] ++ " [oUnit arguments]",
    );
  let _ =
    /* Reset argument counter, to let OUnit reparse arguments */
    Arg.current := 0;
  let suite = build_suite();
  run_test_tt_main(suite);
};
