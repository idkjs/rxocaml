module Observer = {
  module ObserverState = {
    type state('a) = {
      completed: bool,
      error: option(exn),
      on_next_queue: Queue.t('a),
    };

    let initial_state = () => {
      completed: false,
      error: None,
      on_next_queue: Queue.create(),
    };

    let on_completed = s => {
      OUnit2.assert_equal(
        ~msg="on_completed should be called only once",
        false,
        s.completed,
      );
      OUnit2.assert_equal(
        ~msg="on_completed should not be called after on_error",
        None,
        s.error,
      );
      {...s, completed: true};
    };

    let on_error = (e, s) => {
      OUnit2.assert_equal(
        ~msg="on_error should not be called after on_completed",
        false,
        s.completed,
      );
      OUnit2.assert_equal(
        ~msg="on_error should be called only once",
        None,
        s.error,
      );
      {...s, error: Some(e)};
    };

    let on_next = (v, s) => {
      OUnit2.assert_equal(
        ~msg="on_next should not be called after on_completed",
        false,
        s.completed,
      );
      OUnit2.assert_equal(
        ~msg="on_next should not be called after on_error",
        None,
        s.error,
      );
      Queue.add(v, s.on_next_queue);
      s;
    };
  };

  module O = Rx.Observer.MakeObserverWithState(ObserverState, RxCore.DataRef);

  type state('a) = RxCore.DataRef.t(ObserverState.state('a));

  let create = O.create;

  let is_completed = state => {
    let s = RxCore.DataRef.get(state);
    s.ObserverState.completed;
  };

  let is_on_error = state => {
    let s = RxCore.DataRef.get(state);
    BatOption.is_some(s.ObserverState.error);
  };

  let get_error = state => {
    let s = RxCore.DataRef.get(state);
    s.ObserverState.error;
  };

  let on_next_values = state => {
    let s = RxCore.DataRef.get(state);
    BatList.of_enum @@ BatQueue.enum(s.ObserverState.on_next_queue);
  };
};
