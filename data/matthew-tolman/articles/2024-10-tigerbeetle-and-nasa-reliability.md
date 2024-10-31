Last time I posted I talked about Erlang and how it has built-in error recovery to maintain reliable systems. Erlang's error recovery is based on monitoring processes and automatically restarting subsystems to a known good state when a process fails. This happens at the cost of any data inside of the restarted processes. The built-in error recovery allows Erlang to keep functioning even in the face of errors, network partitions, and bugs. It's an interesting paradigm since it approaches the problem differently than more traditional "try/catch what you can and crash otherwise" mentality.

Since that post I've been digging into Go (posts coming) and Zig. The main reason I started with Zig is because of [TigerBeetle](https://tigerbeetle.com/) and their methodology for making reliable systems, which is based on what [NASA does](https://en.wikipedia.org/wiki/The_Power_of_10:_Rules_for_Developing_Safety-Critical_Code) (or at least did). Their approach is the polar opposite of Erlang's approach. Instead of automated error recovery through subsystem restarts, they use strict programming patterns to build analyzable systems which operate in knowable bounds and memory spaces. With these limitations, they're able to prevent many classes of errors.

## NASA's 10 Rules

NASA/JPL Laboratory developed 10 rules for developing safety-critical code. These rules are as follows:

$ Restrict all code to simple control flow (no goto, no recursion)
$ Give all loops a fixed upper bound that can be trivially verified by static analysis
$ Do not use dynamic memory allocation after initialization
$ No function should be longer than what can be printed on a single sheet of paper
$ Code assertion density should average to minimally two assertions per function
$ Declare data objects at the smallest possible level of scope
$ Each calling function must check the return of a returning function, or annotate that it is intentionally ignored. All called functions must validate incoming parameters
$ The use of the preprocessor must be limited to the inclusion of header files and simple macro definitions
$ The use of pointers must be restricted, and no more than one level of dereferencing should be used
$ All code must be compiled with all warnings enabled and all warnings must be resolved

### Ensuring program boundaries

These points are pretty simple, and a lot of them are fairly easy to reason about. The first two points are about code simplicity and avoiding infinite loops. Code which has simpler control flow is simpler to understand and analyse. Additionally, if we give every loop a maximum upper bound, then we won't have an infinite looping scenario. 

No dynamic memory allocations gives an upper bound on how much memory the program can ever use. Since we have a known maximum stack size, and we aren't dynamically allocating any memory, we then will have a known maximum memory usage. We can then allocate enough RAM so that we'll never run out of memory, even in the face of mallicious user input or DOS attacks. We also avoid all memory leaks by simply not allocating memory that can be leaked. 

Avoiding allocations also impacts performance in significant ways. Memory allocators have to make system calls which are notoriously slow. Additonally, many allocators use locks internally so they can be thread safe.. Furthermore, memory allocators also need to use manage memory algorithms to minimize fragmentation, and some also perform garbage collection. All of these factors mean that allocating memory is rather slow (compared to using the stack). The slowness is especially troublesome when dealing with lots of micro allocations. By removing allocations entirely from the main parts of a program, we automatically avoid slowdowns that come with memory allocation.

### Coding style

Point 4 is about code readability. Often, people will come up with asinine rules for code readability such as "no function should ever be more than 5 lines of code" or "code should never be nested". These little rules often sound good out of context, and are easy to remember. However, they are hellish in the context of writing maintainable software. Often, developers will fragment their code over and over again until they have a jigsaw puzzle of thousands of functions and classes. Extremely fragmented code is hard to read and piece together as developers are having to jump from class to class and function to function just to understand what is happening.

In contrast to the typical recommendations, NASA gives a much wider (and more "soft") rule of "fits on a printed piece of standard paper." They don't specify A4 or letter paper, font size, or line height. This fuzziness prevents someone from blocking a PR simply because they counted one too many lines. Additionally, NASA's recommendation is long enough that most devs aren't going obsess over making tons of small functions. But, the guideline is also short enough that devs will avoid multi-hundred line functions. Overall, it's the best code size limitation I've seen.

NASA's next recommendation to use assertions is genius. Often the only assertions in a codebase are test assertions. These assertions are written in tests, for tests, and only run with tests. They also tend to check similar constraints (did this method get called, did this return param match the expected value, etc). However, since they only run when tests are ran, it requires that the tests be both correct, extensive, and have the proper assertions to catch any potential errors that could happen at any part of the tested subsystem. Such a feat rarely (if ever) happens when relying soley on test assertions.

Instead, code should have assertions baked into it which prevent impossible or unexpected situations (aka. bugs). These assertions will run in addition to test based assertions. This allows commonly copied assertions to move from the tests into the code, thus cleaning up the test cases. Furthermore, these code assertions don't just run when running automated tests. They run anytime the program is ran. This includes manual testing by both developers and QA (if you're lucky enough to have them). This means you don't need "rockstar" tests to have safe code. The code is inheritly enforcing safety.

### Scope and validation

Object declarations at the lowest scope makes sense. Don't make a global variable if it's not needed to be global everywhere. Instead, find the smallest subset of code that needs it and scope it there. This helps reduce the error space when race conditions, memory errors, or logic errors happen with a variable. It also helps make sure variables are properly deinitialized when no longer needed.

This rule does not mean "do dependency injection/inversion." Dependency injection violates this rule since the objects are declared in a global scope (the injector) and can be accessed from anywhere (just by asking). Dependnecy inversion may be helpful in following this rule as it allows lower scopes to receive objects from higher scopes. However, dependency inversion does not guarantee that the incoming variables are properly scoped. It's merely a tool for passing scoped objects.

The next rule is a two-parter. When a function takes an incoming parameter, it should always check that it's valid. This means that developers should never assume that code will always take a specific call path (i.e. callers will always call `validate` before calling `process`). Likewise, code shouldn't be discarding values - especially when those are error values (whether returned or thrown). NASA does allow for values to be explicitly marked as ignored, but it must be explicit.

### Preprocessor, Pointers, Compilers

The restricted use of the preprocessor makes total sense for anyone who's done C/C++ code. That said, the idea isn't limited to just C macros. The rationale NASA gives is that non-trivial macros are difficult for developers to understand, and they are difficult for tooling to properly analyse. This rationale for restricting macros should be applied to any type of code generation. This includes compiler plugins (e.g. Java lombok), non-trivial C++ templates, non-trivial Zig comptime, DSLs for generating code, Generative AI, etc. In short, don't be clever or lazy with the code. Be intentional. If something is taking too long to write by hand, then take a step back and vary your approach.

NASA also adds heavy restrictions on pointers. This is again in the interest of preventing bugs. Pointers indicate shared memory, and shared memory can often lead to race conditions (e.g. data race, logical race). Additionally, shared memory requires more complicated lifetime management. The memory cannot be freed before all users have accessed the memory. Since we aren't dynamically allocating memory, that means our shared memory is on the stack. Since the lifetime of stack memory is tied to block scope, developers need to be careful to ensure that the pointers never live past the block scope. By restricting pointers and greatly simplifying the cognitive load, NASA engineers are better able to ensure proper pointer usage and lifetimes.

Finally, compiler warnings. Compilers have progressed a lot at detecting bad practices in their warnings. Do listen to them. If you feel like it's too restrictive, go learn Rust. Afterwards you'll feel like clang/gcc `-Wall -Wextra` compiler warnings are no longer restrictive.

## TigerBeetle Additions

TigerBeetle adds some additional safeguards ontop of NASA's rule of 10. First is simulation testing, which is really impressive. They inject simulated faults in addition to bad data and good data for their program. They also can accelerate time to test for additional edge cases around time passing (e.g. delayed recovery, leap years, end of month, etc). This is really cool, and should be more made commonplace.

Additionally, they have more guidelines such as the following:

$ Avoid compound conditions and prefer nested if/else blocks. Similarly, split long `else if` chains into nested `else { if {} }` trees
$ When writing conditions, do the positive case not the negative case (e.g. `hasAccess()` not `!hasAccess()`)
$ Handle all errors (this is more strict than NASA which allows explicitly ignoring errors)
$ Document why decisions were made
$ Explicitly pass optional options instead of relying on the default values

The first two are readability focused, but I do like them since they play into the capabilities of modern IDEs. Modern IDEs can fold code segments, so organizing code into trees allows developers to fold the sections they aren't interested in. By having the positive case be the primary case, it also allows developers to quickly identify the sections that they can fold. Handling all errors helps prevent bugs and data corruption, documenting the "why" is the most useful part of any documentation, and explicitly passing optional parameters protects developers against a library maintainer changing defaults without notice.

## TigerBeetle and NASA vs Erlang

The rules from TigerBeetle and NASA for reliable systems are at odds with Erlang, even to the point of being unimplementable (especially on the opinions of recursion). And that's for good reason.

Erlang is built around the assumptions that data loss is acceptable and that failure is unavoidable. Erlang was made for telecom (phone services). With phones, if a call is dropped, humans will simply redial. Nothing catastrophic is lost, only time and patience. All that matters is that calling works at some point in the future. Additionally, there are many outside factors that could cause issues, even if the code was future proof. Unreliable service, down cell towers, power outages, busy lines, etc. In these cases, it's more important to have monitoring and fallbacks rather than airtight data preservation. Finally, even though replacing hardware and updating software isn't desireable, both are possible. This means that the cost of getting it wrong is much lower than the cost of going to extremes to guarantee that it's right.

In contrast, NASA doesn't have the same luxuries as telecom. NASA doesn't have extra space probes to reroute inter-space communications to. They also cannot simply (or cheaply) replace space probes at the edge of our solar system. They do have hardware backups built-in to their probes, and they will failover if needed. However, that is the worst case scenario since any failed hardware cannot be replaced. They also want to avoid system restarts when the system being restarted is traveling thousands of miles per hour and may not be able to restablish communications once restarted.

TigerBeetle also differs from telecom in their requirements. For TigerBeetle, data loss is unacceptable, to the point that they are built to handle disk errors at the hardware level. The loosey-goosey "just drop the process data and restart" mentality of Erlang isn't acceptable for TigerBeetle. TigerBeetle needs data reliability and integrity, not just overall system availability.

Because of the difference in assumptions and requirements, we end up with extremely different patterns for reliability (and even definitions of "reliability"). For Erlang, reliability equates to system-wide availability, even when subsystems are failing. For NASA and TigerBeetle, reliability is about data and systemic integrity with knowable behavior in the face of failure and stress.

So, Erlang is not ideal for NASA and TigerBeetle. But, Zig is.

## What Zig provides

Zig provides a lot which makes it ideal for the NASA and TigerBeetle style of coding. First, Zig has lots of assertions built into the language, such as bounds checking. They also have a `std.debug.assert` that can be used for additional assertions.

Zig also requires that all memory allocations are explicit by not providing a global allocator. This means if a library wants to allocate (including the standard library), you must pass it an allocator. Already avoiding memory allocations is much easier since developers instantly know what code they can and cannot use when programming.

Zig also makes errors explicit as values and enforces that errors are ether acknowledged or checked at the compiler level. While this is NASA style instead of TigerBeetle style by default, we can make our main function never return an error to start enforcing TigerBeetle style.

The lack of preprocessor handles NASA's requirement by the letter of the law, but only doing non-trivial usages of Zig's comptime (or avoiding it) handles the spirit of the law. Even when used, Zig's comptime is simply better than C macros. It has better editor support, clearer error messages, type safety, and the ability to add assertions.

These simple features make NASA coding style viable (and often easier) in Zig whereas it's often impractical (or impossible) in other languages. 

## My Experience

The NASA/TigerBeetle style is very draconian, but it provides large payoffs in both data and system integrity and reliability. The times I've followed it fully in Zig I've been confident about my code quality and have avoided several mistakes that would have turned into grueling bug hunts. In contrast, the times I thought I could go without have led to silly, hard to track mistakes and eventual giving in to the style. Code built with the NASA/TigerBeetle style is marvelous to work with. The confidence in correctness has often exceeded the confidence I've had in my Rust code. While not all rules can be easily applied to every language, I recommend that developers apply the parts they can.

