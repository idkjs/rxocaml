/** Synchronized mutable data structure. */;

type t('a);

let create: 'a => t('a);

/** Synchronized getter. */

let get: t('a) => 'a;

/** Non-synchronized getter. */

let unsafe_get: t('a) => 'a;

/** Synchronized setter. */

let set: ('a, t('a)) => unit;

/** Non-synchronized setter. */

let unsafe_set: ('a, t('a)) => unit;

/** Synchronized setter. Returns the value before updating. */

let get_and_set: ('a, t('a)) => 'a;

/** Synchronized update. */

let update: ('a => 'a, t('a)) => unit;

/** Synchronized update. Returns the updated value. */

let update_and_get: ('a => 'a, t('a)) => 'a;

/**
 Synchronized compare and set (CAS).

 [compare_and_set compare_to new_value data] compares current value of [data]
 with [compare_to] value. If they are equal, set the current value to
 [new_value]. Returns the value before updating.
 */

let compare_and_set: ('a, 'a, t('a)) => 'a;

/**
 Synchronized update with precondition.

 [update_if predicate f data] checks [predicate] on the current value of
 [data]. If it is [true], update the current value applying [f]. Returns the
 value before updating.
 */

let update_if: ('a => bool, 'a => 'a, t('a)) => 'a;

/**
 Synchronized function application.

 [synchronize f data] applies [f] to the current value of [data] and returns
 the return value of [f].
 */

let synchronize: ('a => 'b, t('a)) => 'b;
