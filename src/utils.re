let try_finally = (thunk, finally) =>
  try({
    let result = thunk();
    finally();
    result;
  }) {
  | e =>
    finally();
    raise(e);
  };

let current_thread_id = () => Thread.self() |> Thread.id;
