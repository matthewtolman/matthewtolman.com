Back in high school, I worked on robots with a few other students.
When we built our robots we had motors and wheels that were a foot or two apart.
To transfer the energy from our motor to our wheels, we used chains.

~img::src=/imgs/chains-buckets-iteration/robot-wheels-with-chains.svg::alt=Diagram of a robot where the wheel and motor are separated by a large distance::

As the motor spun, it iterated through the links in the chain sequentially.
The motor never skipped a link in the chain, it always iterated in order

When we wanted the robot to go backwards, the motor would change its direction of iteration.
Instead of going clockwise through the chain, it would go counter-clockwise.
Even then, it never skipped a link.

When working on our robots, we often had buckets with parts or other odd ends.

~img::src=/imgs/chains-buckets-iteration/sand-buckets.svg::alt=Buckets filled with stuff::

Whenever we needed a part, we'd go to a bucket and see if it was there.
Usually, we knew which bucket we needed (e.g. fourth one from the right) and we'd go straight to that bucket,
no need to search around.

There isn't an inherit property of buckets which restricts how we could iterate through them.
We could (and did) start our search anywhere, skip a few buckets, etc.

In contrast, due to the nature of how sprockets interact with a chain of links, there is a
restriction on how the motor can iterate through the chain. It can go forwards or backwards,
but it must go in order. It cannot skip parts of the chain.

In computer science terminology, the buckets are like an array and the chain is like a doubly linked list.
With buckets (and arrays), you can go forwards and backwards, but you can also jump around.
Meanwhile, with the linked list you can only go forwards and sometimes backwards.
There is no arbitrary jumping.

Why does it matter if something is array-like or list-like?
Because it dictates how code can be written and how well code performs.

These types of constraints appear in not just data structures, but also in abstractions around data structures.
For example, consider JavaScript's iterator.
It is an abstraction around an underlying data source.

JavaScript iterators have a "next" method which returns both the next item in
the iterator and whether the iterator is finished^[mdn-iterators].
While on the surface this seems perfectly fine, it does have an issue.
If I want to get the 5th item from an iterator, I have to get the 1st item, and then the 2nd item,
and then the 3rd item, and then the 4th item, and then finally I can get the 5th item.
In short, I have to go sequentially through the iterator to get the item that I wanted.
This is like the chain where I have to go through sequentially instead of jumping around.

The lack of jumping around can cause issues when developing libraries (speaking from experience^[hat-js]),
The inability to jump around can cause issues with performance.
If a program needs just the 1,200,000th item, why waste their time iterating through 1,199,999 items?
Arrays allow users (including other program) to jump to the data they want.
Iterators require programs to go sequentially through the data until it finds the data it's looking for.
For large data sets, this can become expensive.

Another problem arises when iterators don't have an end.
Unlike most data structures, iterators allow for a special type of sequence called an "infinite sequence".
An infinite sequence sounds like what it is, an infinite number of values.
If you were to try to convert one of these sequences into an array, you'd get an infinite loop (and use an infinite amount of memory).
The simplest way to make an infinite iterator in JavaScript is to have the "next" function never report that it's done.
The worst part is that there is no indication of whether an iterator is infinite.
The programmer just has to "know" if something is infinite or not (or at least if it can be).
If the programer is wrong, then there's an infinite loop.

These limitations often make iterators less suitable than arrays, which is partly why arrays are still common.

However, there is a way to tweak iterators to fix many of the issues.
The solution comes from the C++ community.

In C++ there are five types of iterators^[cplusplus-iterators]:
* Output
* Input
* Forward
* Bidirectional
* Random Access

Output iterators are for pushing values somewhere, such as pushing strings to stdout.
This is similar to publishers in a pub/sub context.

Input iterators are for receiving values from somewhere, such as receiving keystrokes from the user.
They are similar to subscribers in a pub/sub context.
Input iterators can only get the next value, and they can get values once.
They should not be reused or forked, similar to Java streams^[oracle-java-streams].

Input and forward iterators are the closest C++ has to JavaScript iterators.
Forward iterators are for forward access for the next value.
However, unlike input iterators, a forward iterator can be restarted from the beginning to be reused.
Forward iterators are like the sand in an hour glass.
The sand flows one way to the bottom and doesn't flow up.
But, you can always turn the hour glass over to restart at the beginning.

~img::src=/imgs/chains-buckets-iteration/hour-glass.svg::alt=Image of an hour glass::height=300px::

Bidirectional iterators are like our chain on the robot.
We can go forwards and backwards, but we have to do so sequentially.
It can also be thought of as iterating a doubly-linked list.

~img::src=/imgs/chains-buckets-iteration/bidirectional-iteration.svg::alt=Diagram of a doubly-linked list where nodes are linked both forwards and backwards::

Random access iterators are the most powerful type of iterator in C++.
They allow bidirectional iteration (forwards and backwards), but they also allow skipping around which is very powerful.
Does a program want the 1,200,000th element?
No problem! Just skip directly to it, no need to evaluate the first 1,199,999 elements.
Now do you want jump to the 100th element? Sure! Just skip on over to it.
Furthermore, unlike all other iterators, random access iterators can calculate the distance to any other random access iterator.
For library implementers, this is handy since it allows quickly detecting sizes when partitioning and counting iterators.

With just these tools, more data structures can be efficiently abstracted with iterators.
But C++ isn't done. Remember the issue with infinite sequences?
C++ addresses that too by having iterator ranges.

Instead of just having "is this done?" and "is this not done?", in C++ you can pass a pair of iterators
to define a range. Only care about sorting the first 100 items? Well pass a range that encompasses just those 100 items.
Want a function to guarantee it's input is finite? Have it require an iterator range.

These ranges can be emulated using functions like "drop"^[clojure-drop] and "take"^[clojure-take] from Clojure.
However, the disadvantage of these emulation functions is that those ranges are *implicit*, meaning developers still
need to know if the iterator they're working with is bounded or not.
With C++, ranges are *explicit*, allowing libraries to guarantee that iterators are bounded which removes the burden from the developer.

Finally, there's one last trick C++ developers have, and that is the ability to change algorithms
based on the type of incoming iterator.
Many tasks algorithms that run faster or slower depending on the underlying data structure.
These algorithm differences also translate over to the abstractions we use on top of an algorithm.
With a good abstraction, we can maintain the speed benefit of the underlying data structure.
With a poor abstraction, we lose that benefit since our algorithm code can only "see" a slower data structure.

For example, forward and bidirectional iterators don't know the distance to other iterators.
This means counting the distance between two forward or bidirectional iterators requires traversing
the iterator and counting each step taken.
Getting the distance for forward and bidirections ends up being O(n).
However, random access iterators can calculate the distance between each other, so calculating
the distance between two iterators is simply asking the iterators, which is almost always O(1).

By allowing libraries to adapt their algorithms based on the kind of incoming iterator,
library writers are able to do adaptive performance, hitting the fastest performance they can
inside the constraints they're given.
Without this adaptability, library writers are left with least-common-denominator performance-wise.

JavaScript iterators aren't perfect.
However, with some tweaks, they can be made much better.
The solution isn't to throw out iterators, but to look at what other languages are doing
and then use those ideas to build upon what we have.

@References
* mdn-iterators
  | type: web
  | page-title: Iterators and generators
  | website-title: Mozilla Developer Network
  | link: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Iterators_and_Generators
  | date-accessed: 2022-09-20
* hat-js
  | type: web
  | page-title: @tofurama300/hat-js
  | website-title: NPM
  | link: https://www.npmjs.com/package/@tofurama3000/hat-js
  | date-accessed: 2022-09-20
* cplusplus-iterators
  | type: web
  | page-title: iterator
  | website-title: cplusplus.com
  | link: https://cplusplus.com/reference/iterator/
  | date-accessed: 2022-09-20
* oracle-java-streams
  | type: web
  | page-title: Interface Stream<T>
  | website-title: Java Platform Standard Ed. 8
  | link: https://docs.oracle.com/javase/8/docs/api/java/util/stream/Stream.html
  | date-accessed: 2022-09-20
* clojure-drop
  | type: web
  | page-title: drop
  | website-title: ClojureDocs
  | link: https://clojuredocs.org/clojure.core/drop
  | date-accessed: 2022-09-20
* clojure-take
  | type: web
  | page-title: take
  | website-title: ClojureDocs
  | link: https://clojuredocs.org/clojure.core/take
  | date-accessed: 2022-09-20
