let _ = {
  let enum = BatList.enum(["Hello", ",", " ", "world", "!", "\n"]);
  let observable = Rx.Observable.CurrentThread.of_enum(enum);
  let observer = Rx.Observer.create(s => print_string(s));
  let _ = observable(observer) |> ignore;
  ();
};
