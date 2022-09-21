Back in high school, I worked on robots with a few other students.
When we built our robots we had motors and wheels that were a foot or two apart.
To transfer the energy from our motor to our wheels, we used chains.

~img::src=/imgs/chains-buckets-iteration/robot-wheels-with-chains.svg::alt=Diagram of a robot where the wheel and motor are separated by a large distance::

The chains linked to each other and pushed and pulled on each other whenever our motor spun.
One by one, each chain link would gain energy from the motor and then transfer it to the wheel.
As the motor spun, it iterated through the links in the chain sequentially.
The motor never skipped a chain or jumped around the different parts of the chain.
It simply rotated through the chain in order.

When we wanted the robot to go backwards, the motor would change it's direction of iteration.
Instead of going clockwise through the chain, it would go counter-clockwise.
Even then, it never skipped, always going in sequence.

Another hobby of mine growing up was making sand castles (though I wasn't that great).
One of the big parts of sand castle making is to make sure you have wet sand that will hold together.
Imagine that you are now at the beach, and you're making a sand castle.
You have several buckets lined up, all filled with wet sand.

~img::src=/imgs/chains-buckets-iteration/sand-buckets.svg::alt=Buckets filled with sand on the beach::

Imagine you are standing at one end of the line of buckets.
Whenever you need wet sand, you can go get it from a bucket and use it to make your sand castle.
Naturally, you'd most likely get your wet sand from the bucket nearest to you.
Once that bucket ran out, you could get sand out of the next closest bucket, and so on.
But, there's nothing saying you have to.

If you wanted to, you could get sand out of the second closest bucket and then the fourth closest bucket,
skipping every other bucket.
Or you could get sand from the furthest bucket and then the next furthest bucket and so on.
Or you could roll some dice to see which bucket to grab sand out of.
In all of these bucket scenarios, you still have iteration, but it's very different from when the motor iterated
over the chain.

There isn't an inherit property of buckets in a line which restricts you in how you can iterate
through the buckets of sand. You can skip any number of buckets, move in any order,
wrap around, go backwards, or use pure random chance. You can even start wherever you want!

In contrast, due to the nature of how sprockets interact with a chain of links, there is a
restriction on how the motor can iterate through the chain. It can go forwards or backwards,
but it most go in order. It cannot skip parts of the chain or choose where it starts.
It starts with the chain links it's touching, and then it goes forwards or backwards.

In computer science terminology, the buckets are like an array and the chain is like a linked list
(more accurately, a doubly-linked list).
With the buckets (and arrays), you can go forwards and backwards, but you can also jump around.
Meanwhile, with the linked list you can only go forwards and sometimes backwards.
There is no arbitrary jumping.

Why does it matter if something is array-like or list-like?
Simply because it dictates not just how you write your software, but also the performance of that software.

There's a trend in a lot of programming languages to have iterator support.
Most iterators are modeled on the concept that programs can do one of two things:
 * Get the "next" item in the iterator
 * Check that if we've reached the end of the iterator

For instance, In JavaScript, iterators have a "next" method which returns the next item in
the iterator and whether the iterator is finished^[mdn-iterators].
While on the surface this seems perfectly fine, it does have an issue.
If I want to get the 5th item from an iterator, I have to get the 1st item, and then the 2nd item,
and then the 3rd item, and then the 4th item, and then finally I can get the 5th item.
In short, I have to go sequentially through the iterator to get the item that I wanted.
In other words, JavaScript iterators are like linked lists and not like arrays.

While it may not seem like a big deal on the surface, it can be once we dig deeper.
The first question to ask ourselves is, why would we use an iterator over an array?
Arrays are built in, don't require providing a next method, and allow arbitrary access.
Arrays also require all values to be evaluated prior to array creation.
This last property of arrays is the crux of an issue.

If there is a series of results and getting any single result is expensive, it can cost a lot
to populate arrays of the results, especially if the user doesn't need all the results at once.
Even if getting individual results may not be expensive, if there are a too many results then
they may not even all fit in memory.
In either of these scenarios, iterators provide an alternative solution.

Instead of populating an entire array, programmers create a function that describes how to get the next
value.
The code the calls that function only if needed.
Once that method is called, a single item is evaluated and returned.
For expensive operations, only the values required are ever calculated.
For large data sets, only a single item lives in memory.
It also helps when developing libraries.
Instead of requiring a specific type of data structure, like an array or a list,
library authors can operate on an iterator to work on any type of data structure.
Additionally, when library authors create a new type of data structure, they can provide an iterator
to work with existing libraries.
It's supposed to be a win-win.

So, why then aren't most developers using iterators?
One reason is that there isn't much tooling support yet.
If you want to sort an array, that's built in. 
If you want to get the middle of an array, that's pretty easy.
If you want to reverse an array, that's also easy.
But if you want to do any of that with an iterator, you're going to have some problems.
Those types of operations aren't built in.
There are libraries which can work with iterators (such as my own personal library^[hat-js]),
but there are caveats due to how iterators typically work.

A lot of fast algorithms, like quick sort, are often designed around arrays where you can jump
around or divide the array arbitrarily.
They work well with anything array-like, but start to break down with anything list-like.
It is possible to convert some iterators to arrays and then use the array algorithms, but
that means both evaluating every item (which we were trying to avoid with iterators) and storing all
of them in memory (again, trying to avoid) for a single operation.
At that point, just use the arrays.

A problem is the inability to skip around.
If a program needs just the 1,200,000th item, why waste their time iterating through 1,199,999 items?
Arrays allow users (including other program) to jump to the data they want.
Iterators require programs to go sequentially through the data until it finds the data it's looking for.
For large data sets, this can become expensive.

Another problem arises when iterators don't have an end.
Unlike most data structures, iterators allow for a special type of sequence called an "infinite sequence".
An infinite sequence sounds like what it is, an infinite number of values.
If you were to try to convert one of these sequences into an array, you'd get an infinite loop (and use an infinite amount of memory).
The simplest way to make an infinite iterator in JavaScript is to have the "next" function never report that it's done.
So long as the "next" function never says it's done, it's infinite.
JavaScript will always treat it like there's another value, no matter how many values it pulls.
The worst part is that there is no indication of whether an iterator is infinite or not by inspecting the value.
The programmer just has to "know" if something is infinite or not (or at least if it can be).

Finally, many iterators often can't fully support all the ways a datastructure can be used.
A lot of iterators can't support the skipping around that arrays and maps support, so
any data structure that supports those types of iteration can't be accurately represented.
Instead, everything is often reduced down to a linked list (and often a singly-linked list at that).

These limitations often make iterators less suitable than arrays, which is why arrays are often
so common.

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

Forward iterators are for forward access for the next value.
However, unlike input iterators, a forward iterator can be restarted from the beginning to be reused.
Forward iterators are like the sand in an hour glass.
The sand flows one way to the bottom and doesn't flow up.
But, you can always turn the hour glass over to start at the beginning.

~img::src=/imgs/chains-buckets-iteration/hour-glass.svg::alt=Image of an hour glass::

Input and forward iterators are the closest C++ has to JavaScript iterators.
Everything else C++ has is more powerful and can better represent different types of data structures.

Bidirectional iterators are like our chain on the robot.
We can go forwards and backwards, but we have to do so sequentially.
It can also be thought of as iterating a doubly-linked list.

~img::src=/imgs/chains-buckets-iteration/bidirectional-iteration.svg::alt=Diagram of a doubly-linked list where nodes are linked both forwards and backwards::

Random access iterators are the most powerful type of iterator in C++.
They allow bidirectional iteration (forwards and backwards), but they also allow skipping around.
This skipping around is very powerful.
Does a program want the 1,200,000th element?
No problem! Just skip directly to it, no need to evaluate the first 1,199,999 elements.
Want to now jump to the 100th element? Sure! Just skip on over to it.
This skipping allows random access iterators to model arrays, maps, and more.
Furthermore, unlike all other iterators, random access iterators can calculate the distance to any other random access iterator.
For library implementers, this is handy since it allows quickly detecting sizes when partitioning and counting iterators.

With just those tools, already more algorithms and data structures can be efficiently represented
using iterators as an abstraction. However, C++ isn't done. Remember the issue with infinite sequences?
C++ addresses that too by having iterator ranges.

Instead of just having "is this done?" and "is this not done?", in C++ you can pass a pair of iterators
to define a range. Only care about sorting the first 100 items? Well pass a range that encompases those 100 items.
What about summing items 300-500? Well pass a range for that too.

These ranges can be emulated using functions like "drop"^[clojure-drop] and "take"^[clojure-take] from clojure.
However, the disadvantage of clojure's functions is that those ranges are *implicit*, meaning developers still
need to know if the iterator they're working with is bounded or not.
With C++ (at least C++17 and earlier), ranges are *explicit*, allowing libraries to guarantee
that iterators are bounded, which removes the burden from the developer.
That said, the explicit nature does make the code verbose, so C++20 and newer have opted to infer
the range bounds from the underlying source.
Algorithms still able to require a bounded iterator, but instead of the programmer passing in both
boundaries, the code infers the boundaries from the data source.
It's a nice little evolution that solves the infinite sequence problem while making code cleaner.

Finally, there's one last trick C++ developers have, and that is the ability to change algorithms
based on the type of incoming iterator.
Many tasks have both faster and slower versions of an algorithm, depending on the underlying data structure.
For example, forward and bidirectional iterators don't know the distance to other iterators.
This means counting the distance between two forward or bidirectional iterators requires traversing
the iterator and counting each step taken.
Getting the distance for forward and bidirections ends up being O(n).
However, random access iterators can calculate the distance between each other, so calculating
the distance between two iterators is simply asking the iterators, which is almost always O(1).

By allowing libraries to adapt their algorithms based on the kind of incoming iterator,
library writers are able to do adaptive performance, hitting the fastest performance they can
given the constraints they're given.
Without this adaptability, library writers are left with least-common-denominator performance
where performance is constrained to the slowest iterator available.

Iterators aren't perfect, especially in JavaScript.
However, with some tweaks, they can be made much better and can better serve
developers and library writers.
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
