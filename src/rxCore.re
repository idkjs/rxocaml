type observer(-'a) = (unit => unit, exn => unit, 'a => unit);

type subscription = unit => unit;

type observable(+'a) = observer('a) => subscription;

type notification('a) =
  | OnCompleted
  | OnError(exn)
  | OnNext('a);

type subject('a) = (observer('a), observable('a));

module type MutableData = {
  type t('a);

  let create: 'a => t('a);

  let get: t('a) => 'a;

  let set: ('a, t('a)) => unit;
};

module DataRef = {
  type t('a) = ref('a);

  let create = v => ref(v);

  let get = r => BatRef.get(r);

  let set = (v, r) => BatRef.set(r, v);
};
