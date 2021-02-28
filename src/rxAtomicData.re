type t('a) = {
  mutable data: 'a,
  mutex: Mutex.t,
};

let with_lock = (ad, f) => BatMutex.synchronize(~lock=ad.mutex, f, ());

let create = initial_value => {data: initial_value, mutex: Mutex.create()};

let get = ad => with_lock(ad, () => ad.data);

let unsafe_get = ad => ad.data;

let set = (value, ad) => with_lock(ad, () => ad.data = value);

let unsafe_set = (value, ad) => ad.data = value;

let get_and_set = (value, ad) =>
  with_lock(
    ad,
    () => {
      let result = ad.data;
      ad.data = value;
      result;
    },
  );

let update = (f, ad) => with_lock(ad, () => ad.data = f(ad.data));

let update_and_get = (f, ad) =>
  with_lock(
    ad,
    () => {
      let result = f(ad.data);
      ad.data = result;
      result;
    },
  );

let compare_and_set = (compare_value, set_value, ad) =>
  with_lock(
    ad,
    () => {
      let result = ad.data;
      if (ad.data == compare_value) {
        ad.data = set_value;
      };
      result;
    },
  );

let update_if = (predicate, update, ad) =>
  with_lock(
    ad,
    () => {
      let result = ad.data;
      if (predicate(ad.data)) {
        ad.data = update(result);
      };
      result;
    },
  );

let synchronize = (f, ad) => with_lock(ad, () => f(ad.data));
