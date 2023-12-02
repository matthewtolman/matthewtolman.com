In a previous post I talked about [[article:what-is-property-testing]] In that post, I discussed the difference between
property tests and more traditional example-based testing, how to write property tests, and the shortcomings of property
tests. If you'd like to learn more about property testing, then go give that article a read.

In this post, I'll discuss my experience making [C++ Property Testing](https://gitlab.com/mtolman/c-property-testing), [@matttolman/prop-test](https://gitlab.com/mtolman/js-prop-tests/), and [Test Jam](https://gitlab.com/mtolman/test_jam) which aims to be the successor to the previous two.

[toc]

## Defining the Requirements

Many property testing frameworks try to be a port of Haskell's QuckCheck^[quickcheck]. However, while that seems specific, it doesn't really help in defining what a property testing framework should do. I spent quite a bit of time trying looking through several different libraries, and I realized that many of them vary widely in features that are available (shrinking vs non-shrinking, input generators, syntax, etc). So I started taking notes on similarites to create my list. The bare minimum which I found among property testing framework is:

* Procedurally generate "random" inputs
    * Must be deterministic (aka. if we set the same seed we get the same output)
* Pass that input into developer-defined test cases
* Run the developer-defined test cases with hundreds to thousands of different inputs

Those are the bare minimum requirements. Those requirements do have a usability issue when it comes to test failures though. Generally, property tests find failures with very complex input (massive strings, arrays with many items, etc). The next tier of frameworks usually have the following capabilities:

* Detect when there is a failed test case
* Find some sort of "simplified" input that still fails but is much less complex than the original failing input

This gets us much closer to a user-friendly framework. Now when a test fails, developers can debug their test.

However, that's still not enough. As framework developers, we cannot possibly create every type of generator that will ever be needed. So we need to add another feature to the list:

* Allow developers to create their own generators whose output can be simplified

These are the main features of a property testing framework. The features I'm going to focus on the most are the generators and simplifiers since the principles behind those work for every language. As far as defining tests, running them, and detecting failures, well that depends on the test framework and programming language being used. I'm not going to cover them here, though I might in the future.

## Simplifying Inputs

Simplifying inputs was my biggest requirement, and how I simplified inputs would steer how I generated values. So I decided to start with the idea of input simplification and work with a few small generators to cement my concept.

To give an idea of why simplifiers are nice, let's consider the following code:

```js
function isValidPassword(pwd) {
    const foundCategories = {}
    for (const ch of pwd) {
        if (invalidChars.indexOf(ch) >= 0) {
            return false
        }
        const category = characterCategory(ch)
        foundCategories[category] = true
    }
    
    for (const category of requiredCategories) {
        if (!(category in foundCategories)) {
            return false
        }
    }
    return true
}
```

If we ran the above code through a property test framework, we'll end up with failed input similar to the following:

```text
LFAIoisdufl3jr2oi($#234k32049KJJ>,.OSIUOFul3rkfs)sfldksjli34o*u2ljd324 LAJLIJWLfW:ejfl u3o(8o3jl43)
```

That's some long text. What's worse, if that piece of text was supposed to pass, it'll be really hard to figure out where the failure is. Is it because we accidentally disallowed a specific character? Is it because we're misclassifying a character? Is it because we made a category required that doesn't exist? Stepping through the code is going to take a while, so we'd have to come up with other debug strategies (such as add print statements).

However, with a simplifier, we'd also get something like the following:

```text
LF2149#
```

That second input example is a lot more concise, and it is a lot easier to debug. We could reasonably step through every character iteration and see what the failure is.

This type of simplification becomes a lot more necessary as data structures gets more complicated. Imagine generating an object with arrays of objects with arrays of objects with multiple strings as values. A failure found by the framework could have thousands of strings inside hundreds of nested objects. But the failure may be happening because one string has the character `a`. If we had a decent simlifier, we could find the bug a lot quicker.

This whole project seemed simple enough, so I picked a programming language that I knew but didn't work with at work and started coding (in this case, I went with C++).

It turns out though, that simplifying random data isn't straightforward, so I had a lot of misteps along the way. I'll go through my different missteps.

### Attempt 1: Ordered Sequence

For my first attempt, I tried to solve simplification and value generation simultaneously. My idea was that my property testing framework was going to generate a random 64-bit number. That number would then be passed to my generator code, which would return a value corresponding to that number. My generator code would be deterministic, so the same input gives the same output - that way if you fixed the random number generator's seed you'd be able to recreate a run.

I then took this idea a step further. I wanted to divide the generator's input number range into simple values and complex values. That way, all the framework needed to do to simplify would be to pass in smaller numbers. And to get more complex results, it just needed to pass in larger numbers.

~img::src=/imgs/writing-property-test-library/simple-complex.svg::alt=Diagram showing smaller numbers creating simpler values and larger numbers creating more complex values::height=300px:;

I could then use this property to ensure I tested with both simple and complex cases. I could also do a binary search through the entire space to find the most simple value which still fails.

I started my attempt with integers. Unsigned nubers were easy since I just needed to cast them. Signed numbers were a little trickier. I took inspiration from how mathematical mappings between signed and unsigned numbers, and I mapped my numbers like so:

| Signed Number | Unsigned Number |
|---------------|-----------------|
| 0             | 0               |
| 1             | 1               |
| -1            | 2               |
| 2             | 3               |
| -2            | 4               |
| ...           | ...             |

This is where the definition of "simpler" starts to break down. Here I'm saying that smaller magnitudes is simpler than larger magnitudes, and that positive numbers are simpler than their negative counterpart. That may or may not be true. And the definition of "simpler" is going to break down a lot throughout this project.

My idea broke down even more with floating point numbers. My initial idea was to simply map my unsigned integers to rational numbers. That's possible, but I'd be limited to a small subset of numbers. My "simpler" definition broke down here again since it's arguable that whole numbers are simpler than fractional numbers, so my idea may not result in a strict "simpler -> complex" ordering. I also ran into some other issues, so I put this on hold and moved to strings.

I had a simple idea for strings. There would be an alphabet of possible characters (e.g. a-z). I would then do a base encoding of the input integer into that alphabet. This would let me theoretically emit any possible string with my alphabet. At first, this sounded like a great idea. Until I realized it could only result in short strings. To get an idea of how short, let's look at some different encodings for the maximum value of an unsigned 64-bit integer (18,446,744,073,709,551,615).

| Encoding | Max number of Characters |
|----------|--------------------------|
| Binary   | 64 |
| Base10   | 20 |
| Hex      | 16 |
| Base32   | 13 |
| Base64   | 12 |

That's not a lot of text. It gets even worse when we allow all ASCII characters or all Unicode characters. It's just not a lot of text characters. What's worse, at some point I may never be able to create a string which passes a minimum text length requirement. It also means I could only catch bugs with small strings. If there's a failure for text over 140 characters (the original Tweet size), then I'd never find it.

After a couple of different ideas, I decided to scrap the "simple to complex" range idea. What I needed was a way to generate more "random" data from a single number, that way I could have random lengths with random characters. It would mean that I'd need a different simplification algorithm to find the simplest failing input, but I had another idea for that.

### Attempt 2: Depth-First Simplification

My next attempt was to look at the problem of simplification like it was a maze. Each possible simlification was another branch, and my goal was to get from the failing value to a passing value. Once I got to a passing input, I would return the last seen failing step.

~img::src=/imgs/writing-property-test-library/maze.svg::alt=Image of a maze where the squares next to the goals will be what is returned::height=760px:;

My simplifier would take in a failing value and it would return an array of possible branches. For instance, the string `abk34jsliaa` would return the following:

```js
[
    "bk34jsliaa", // remove first char
    "ak34jsliaa", // remove second char
    // ...
    "abk34jslia", // remove last char
    "aak34jsliaa", // replaced b's with neighbor
    "abb34jsliaa", // replaced k's with neighbor
    "abkk4jsliaa", // replaced 3's with neighbor
    // ...
]
```

The above shows two different simplification categories: character removal and neighbor replacement. The idea was that a shorter string is simpler than a longer string, and that a string with less distinct characters is simpler than a string with more distinct characters. The simplifier didn't need to know which would get us closer to the goal. It just needed to give us options, and our search algorithm would get us to the goal.

This also meant that I had an n-dimensional search to do, where n depended on the value being simplified. For this, I pulled in graph search algorithms since graphs can represent n connections.

My initial attempt simply used depth-frist search. I chose depth-first since I didn't care about finding the shortest path to a successful value - just any path. It worked okay for small values, and then I ran into some issues.

The first issue was that some of my simplifiers could result in infinite loops. This was my fault because I designed the branches poorly for signed numbers. I decided that "closer to zero" was simpler, but instead of just returning the single branch that moved one closer to zero, I returned a branch that incremented the number and a branch which decremented the number. This meant that my stack would grow as follows:

```text
5
4, 6
3, 5, 6
2, 4, 5, 6
1, 3, 4, 5, 6
0, 2, 3, 4, 5, 6
1, 3, 3, 4, 5, 6
...
```

That ended up creating an infinite loop that took infinite memory. But it also showed me a bigger flaw, which I didn't fully internalize at the time. One of my requirements was to allow other developers to easily create their own custom generators and simplifiers. And yet I, the library designer and creator, was struggling to create generators and simplifiers for my own library. If I couldn't do it, how could I expect anyone else to do it?

So I went back to the drawing board.

### Attempt 3: Breadth-First Simplification

As I was thinking about what to do next, I thought that maybe the issue was I was thinking about things wrong. In a normal maze, there is only one goal (one success). However, in my case there were potentially many different success states.

~img::src=/imgs/writing-property-test-library/maze2.svg::alt=Image of a maze where there are multiple goals::height=760px:;

With a depth first search, I could be trying to find "a" solution, but I could easily go down a path that would lead no where while there was a solution right next to me. Yes I wanted to find any solution, but more accurately I wanted to find the nearest solution. So I switched from depth-first to breadth-first by switching my stack to a queue.

I also still had the issue of infinite searches. To ensure there wasn't an infinite search, I added a max iteration count (around 8,000). I would then return the "simplest" value I had found if I hit that cap. I also started tracking which values I searched (when possible - not all values are comparable/hashable). These changes gave me immediate improvements, even with my broken simplifiers.

I then went about changing my simplifiers to not create infinite loops. Everything seemed to work, until I hit another snag. My iteration limit was now preventing me from finding the simplest value. This was since many of my simplifiers had a lot of branches. For instance, let's take a look at my string simplifier. I had two simplification categories: character removal and neighbor replacement. For the string `abcd`` I would end up with the following branches:

```js
[
    "abc",
    "abd",
    "acd",
    "bcd",
    "aacd",
    "abbd",
    "abdd"
]
```

That's 7 branches for a 4 character string. And this number continues to explode.

For each of the above branches, I got another 5 branches. For each of those branches, I got another 3 branches. And for those branches, I'd get another 1-2 branches. That's around 100 branches for a 4 character string. What's worse was that the growth rate wasn't exponential, it was closer to factorial (or possibly even \(n^n\)). It was really bad.

So I started optimizing my simplifiers. I began skipping steps, cutting out branches, doing multiple simplification steps at once, etc. Eventually most of my simplifiers stopped having multiple branches. At the same time, I had seen my "simplest" value drift from the "true" simplest value, but it was close enough.

I felt like I was starting to get closer to a better approach, but I was caught up in all of my legacy C++ code and my C++ way of thinking. I needed a new perspective, so I switched langauges to TypeScript and started fresh.

### Attempt 4: Generators

In TypeScript land, I had a few more tools built into the language. The first tool I wanted to try was generators. I was moving mostly in a straight line, so there wasn't a need to do branching. However, I also knew that some of my simplification paths were incredibly long, so I didn't want to take up a lot of time and memory computing thousands of elements at once. Generators seemed like a good middle ground.

With generators, my string simplifier became something like this:

```js
function* simplifyString(index) {
    let val = generateString(index)
    while (val.length > 0) {
        val = val.slice(0, -1)
        yield val
    }
}
```

I also went from `O(n!)` branches to `O(n)`, which was a lot more manageable. If needed, I could do `O(log(n))` by simply cutting the string in half each time. That change would be as simple as changing my parameters to the `.slice` call. All of my other simplifiers became simpler too.

Additionally, my simplification function went from something like the following:

```js
function simplify(index, simplifier, predicate) {
    const simplification = simplifier.start(index)
    let simplestValue = simplification.value
    const queue = new Queue()
    queue.addAll(simplification.branches())

    let curIter = 0
    while (!queue.isEmpty() && curIter++ < maxIters) {
        const current = queue.pop()
        const next = current.next()
        if (predicate(next.value)) {
            return current.value
        }

        if (simplifier.isSimpler(next.value, simplestValue)) {
            simplestValue = next.value
        }
        queue.addAll(next.branches())
    }

    return simplestValue
}
```

to the following:

```js
function simplify(index, simplifier, predicate) {
    let curIter = 0
    let lastVal = undefined
    for (const val of simplifier.simplify(index)) {
        if (++curIter >= maxIter || predicate(val)) {
            // Return the previous element on a success or max iteration hit
            // (will never reurn the 8000th element, only the 7999th at most)
            return lastVal
        }
        lastVal = val
    }
}
```

The amount of complexity and overhead I had dropped tremendously. I excitedly ported the idea over to C++ by creating my own lazy sequence and removing the branches. Now that I had a simplifier solution which worked, I could focus on generating data.

## Generating useful data

One of the shortcomings I had seen with a lot of property testing frameworks was that they were primarily focused on purely random data. For simple data types, like numbers, this works really well. For structured data, like URLs, email addresses, and names, this approach really suffers. Structured data is only pseudorandom. There's a lot of patterns and predictability involved.

To demonstrate this, let's consider how we would create a random ASCII string of any length and any value. Here's some code that does that in C++.

```cpp
auto random_string() -> std::string {
    auto res = std::string();

    // just using rand for demonstration purposes
    res.resize(rand() % 10000);
    for (auto& ch : res) {
        // Only valid 7-bit ASCII characters are allowed
        // Anything with 8-bit enters specialized encoding territory
        ch = static_cast<char>(rand() % 128);
    }
    return res;
}
```

The above code could theoretically create any ASCII strings - like a name, email, or a url. However, in practice it just creates a bunch of gibberish. 

Another common string generation technique is to have a set of allowed characters and then to randomly generate from those characters. The code looks like the following:

```cpp
auto random_string_with_chars(const std::string& validChars) -> std::string {
    if (validChars.empty()) {
        return "";
    }

    auto res = std::string();

    // just using rand for demonstration purposes
    res.resize(rand() % 1000);
    for (auto& ch : res) {
        ch = validChars[rand() % validChars.size()];
    }
    return res;
}
```

This allows us to create strings which better match a category, but are still too random to match structured data.

The issue is that the above two methods are typically all that's given to generate random strings. And it sucks. Especially since most property tests expect new generators to be composed of existing generators, rather than built from the ground up. It's also frustrating since it's generally the same types of structured data that needs to be generated for each project (emails, names, etc). There have been times I wanted to use property tests at a new workplace or on a new project, but I didn't want to have to learn how to rewrite the same generator in a new programming language for a new framework. So instead, I just didn't use property tests.

However, this time I was creating my own framework. This time I could build those generators into my framework. So I started doing just that.

When there was a specification, I tried to include all the variations the RFC allows - while allowing developers to opt out of some features. For instance, the SMTP address specification is pretty complicated, allowing for quoted and unquoted email addresses, IPv4 hosts, IPv6 hosts and more^[email-addresses]^[email-address-2]. The following are all valid email addresses:

```text
{hello}@example.com
" hello john "@example.com
john.doe@example.com
"mr..jack.doe"@example.com
hello!#$%&'*+-/=?^_`{|}world~@[192.168.0.1]
excited!user!wow@excited-user.example.com
"yes.(this),:;<is>[actually]#valid\"just\"@why?"@[127.0.0.1]
dontforgetipv6@[IPv6:2001:0db8:85a3::8a2e:0370:7334]
johndoe+34234@example.com
```

Thats a lot of very different looking emails, and most of them are in a format different from what is typically tested (i.e. `myuser12342@example.com`). Some of these may pass the random email regex usually copied from stack overflow, others may not. Some may pass that regex but be undesireable (such as the ones with a loopback IP, or even any IP address). Others often have a special meaning, such as emails with a plus (`+`) often being aliases for an inbox.

These sorts of details and differences present additional decisions to the application developer. But many are unaware these options exist, so they could end up accepting email formats that the rest of their system can't handle.

My solution was to have the full specification as the default, and then to provide flags to turn off features. Here's some example generator declarations from my C++ header file.

```cpp

namespace test_jam::gens::internet {
    /**
     * @return Generator to make IPv4 addresses 
     */
    auto ipv4_addresses() -> common::no_shrink_generator<std::string>;

    /**
     * Option flags for IPV6
     */
    enum Ipv6Flags : unsigned {
        IPV6_BASIC = 0x0,
        IPV6_ALLOW_IPV4 = 0x1 << 0,
        IPV6_ALLOW_FOLDING = 0x1 << 1,
        IPV6_SHRINK_ZEROS = 0x1 << 2,
    };

    /**
     * @param flags Flags for IPv6 features to turn on. Default is all are on
     * @return Generator to make IPv6 addresses
     */
    auto ipv6_addresses(unsigned flags = ~0u) -> common::shrink_generator<std::string>;

    // ....
}
```

The actual generators themselves weren't too complicated. I had two classes for handling my generators: shrink and no shrink. Shrink generators could be simplified, while no shrink generators couldn't be. This was since there were some data types where the question of a "simpler value" didn't make sense (e.g. how do you simplify `10.0.0.1`?). The shrink generator took two callables, a generator and a simplifier, while the no shrink generator took a single callable, a generator.

```cpp
template<typename T>
struct no_shrink_generator {
    std::function<T (TEST_JAM_SEED_TYPE)> genFunc;

    [[nodiscard]] auto create(TEST_JAM_SEED_TYPE seed) const -> T { return genFunc(seed); }
    [[nodiscard]] auto simplify(TEST_JAM_SEED_TYPE seed) const -> sequence<T> { return {[]() -> std::optional<T> {return std::nullopt;}}; }
};

template<typename T>
struct shrink_generator {
    std::function<T (TEST_JAM_SEED_TYPE)> genFunc;
    std::function<sequence<T> (TEST_JAM_SEED_TYPE)> simplifyFunc;

    [[nodiscard]] auto create(TEST_JAM_SEED_TYPE seed) const -> T { return genFunc(seed); }
    [[nodiscard]] auto simplify(TEST_JAM_SEED_TYPE seed) const -> sequence<T> { return simplifyFunc(seed); }
};
```

This meant that all I had to do to create a new generator was to define a callable and return it. Below is my IPv4 generator:

```cpp
auto test_jam::gens::internet::ipv4_addresses() -> common::no_shrink_generator <std::string> {
    return {
            .genFunc=+[](size_t seed) -> std::string {
                TestJamString str;
                test_jam_internet_ipv4(&str, seed, nullptr);
                defer { test_jam_free_string_contents(&str, nullptr); };
                return utils::to_str(str);
            }
    };
}
```

One thing you may have noticed in my C++ code I'm not using inheritance. I didn't feel a need to. Instead, I just used templates and `static_assert`s to pass around my generator types. This was great since other devs don't have to use my base generator classes. Instead, they can create their own class hierachy if they'd like and yet still be able to use my framework.

For TypeScript, I did use inheritance. My base classes are as follows:

```typescript
export interface Generator<T> {
    create(seed?: number): T;
    simplify(seed: number): Iterable<T>;
}

export abstract class NoShrinkGenerator<T> implements Generator<T>{
    public abstract create(seed?: number): T;

    public simplify(seed: number): Iterable<T> { return []; }
}

export abstract class ShrinkGenerator<T> implements Generator<T>{
    public abstract create(seed?: number): T;

    public abstract simplify(seed: number): Iterable<T>;
}
```

I still use an interface at the root, so technically developers don't have to use my class hierarchy. The main reason I used inheritance in TypeScript is I had gotten tired of template programming, and I didn't want to have to repeat all of the template work in TypeScript's generics. So I opted for a more straightforward inheritance hiearchy. Is it better? I personally don't think so, but I don't see myself trying to change it.

## Conclusion

It's been a journey getting this far on my project. As I was working, I had difficulty finding any other articles about why property testing frameworks worked the way they did. I thought that I would at least share my journey on how I got to where I am for my framework.

There were other things I may talk about at some point too, such as my journey with different PRNGs, my attempts at WASM, my scrapped data versioning logic, and making a zero-allocation C API. For now though, this was the main journey I wanted to share.

@References
* quickcheck
  | type: web
  | link: https://hackage.haskell.org/package/QuickCheck
  | page-title: QuickCheck: Automatic testing of Haskell programs
  | website-title: Hackage
  | date-accessed: 2023-10-26
* strgen
  | type: web
  | link: https://github.com/miner/strgen
  | page-title: strgen
  | website-title: GitHub
  | date-accessed: 2023-11-26
* clojure-spec
  | type: web
  | link: https://clojure.org/about/spec
  | page-title: clojure.spec
  | website-title: Clojure
  | date-accessed: 2023-11-26
* email-addresses
  | type: web
  | link: https://en.wikipedia.org/wiki/Email_address#Syntax
  | page-title: Email Address
  | website-title: Wikipedia
  | date-accessed: 2023-10-26
* email-address-2
  | type: web
  | link: https://www.rfc-editor.org/rfc/rfc5322#section-3.4
  | page-title: Internet Message Format
  | website-title: RFC Editor
  | date-accessed: 2023-10-26