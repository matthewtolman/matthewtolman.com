Recently I've been exploring Erlang and the OTP. As I've been trying things out, some aspects of the design Erlang and the OTP have finally started to click. I'm writing this blog post about my experiences with Erlang and the OTP so far.

[toc]

## Working with immutable names

In quite a few functional languages, only the values are immutable. Variable names can be rebound as needed, which is great when working in the REPL. Languages like Clojure and Elixir allow variable rebinding which makes REPL-driven developoment really nice. For instance, you could have a REPL session like the following in Elixir:

```elixir
iex> a = 3 + 4
7
iex> b = 9 - 1
8
iex> a = a + b
15
```

This becomes extremely useful when reloading code in the REPL since you can just rerun previous statements. For instance, let's say I'm writing code which generates a large map of user data, and I'm storing the result in a variable so I can inspect a single element. Let's also say I have a bug in my code where the name is lowercased when it should be uppercased. In Elixir, I can have a REPL session like the following

```elixir
iex> user = Users.load(148)
%User<id: 1, status: :active, tags: ["user", "admin", "flagged"], ...>
iex> user.name
"harry potter"    # <-- This is incorrect, should be uppercased
# I do some fix in the code
iex> recompile() # <-- Reloads code after fix
:ok
iex> user = Users.load(148) # <-- simply hit "up" arrow 3 times
%User<id: 1, status: :active, tags: ["user", "admin", "flagged"], ...>
iex> user.name
"HARRY POTTER" # Fixed
```

Erlang doesn't allow that. Once a variable name is bound, it stays bound. This is due to Erlang's pattern matching. Pattern mattching in Erlang allows for matching to either an unbound variable (in which case the variable becomes bound) or to the value of a variable that's already bound. The syntax is exactly the same for either case. The only thing that matters is whether the variable has already been bound in the context or not. For instance, consider the following code.

```erlang
match_ex(a) ->
  SpecialCase = 42,
  case do_something(a) of
    {ok, SpecialCase} -> special;
    {ok, Case} -> Case
  end.
```

In the above code, the first case matches the tuple `{ok, 42}` since the variable `SpecialCase` is bound to 42. The second case matches any tuple where the first element is `ok`, and it will bind the second element to the variable `Case`. This does make the actual coding of match statements a little easier since there won't be any accidental rebinding of a variable name. Of course, there's still the issue of a good REPL experience.

For a long time, my workaround in the REPL was to add an incrementing number to the end of all of my variables. It was really clunky and frustrating, and one of the reasons I didn't stick around with Erlang for very long. However, I recently found the `f` function. The `f` function will forget variable bindings (at least inside the REPL, I've never tried it in actual code). If you don't give it any parameters, then it will forget all variable bindings, which is a nice way to "reset" a REPL. You can also give it the variable binding you wanto to forget (e.g. `f(Foo)`). This will forget just that one variable binding. This means that our Erlang session with buggy code would look something like the following:

```erlang
1> User = users:load(148).
{user, {1, active, [<<user>>, <<admin>>, <<flagged>>]}, ...}
2> users:get_name(User).
"harry potter" % This is incorrect, should be uppercased
% I do some fix in the code
3> r(). % <-- Reload code after fix
ok
4> f(User). % Forget the user binding
ok
5> User = users:load(148).
{user, {1, active, [<<user>>, <<admin>>, <<flagged>>]}, ...}
6> users:get_name(User).
"HARRY POTTER" % Fixed
```

## Power of Recursion

Erlang doesn't really have loops. It has list comprehensions (similar to Python), but otherwise it relies heavily on recursion to do loops. This is both frustrating and genius at the same time.

It's frustrating at first because you have to rethink how to write code. Instead of writing nested for loops, you instead write multiple functions and then have each function recurse. It's also a big odd since in many languages this kind of code writing this could lead to stack overflows as more stack frames are created. Erlang provides tail call optimizations, so it's not going to blow the stack.

Recursion is also how we normally get past the "cannot rebind variable names" rule. Instead of trying to rebind a variable (or forget the old value), we simply recurse with the new value. This allows us to do loops where we "increment" an iterator by recursing, like so.

```erlang
% Equivalent to
% function (End) {
%  for (let x = 0; x < End; ++x) { console.log(x) }
% }
loop(End) ->
  loop(0, End).
loop(X, End) when X < End ->
  io:format("~p~n", [X]),
  loop(X + 1, End);
loop(_, _) ->
  ok.
```

By itself, this isn't all that remarkable. However, Erlang takes it a step further. Typically, most function calls are a simple stack frame creation followed by a jump to an address in memory where instructions are ran. However, the BEAM is setup so that function calls can also double as "upgrade points," allowing code to be upgraded at runtime. During an upgrade, he VM loads the new code in parallel to the old. Then, whenever a fully-qualified function call is made (module name + function name), the VM will redirect those function calls` to the newest version of the code code. This includes recursive function calls (so long as they're fully qualified). This allows Erlang to do things like upgrade infinite loops mid-loop, such as below.

```erlang
-module(example).
-export([loop/0]).

loop() ->
  io:format("Looping~n"),
  % Since this is fully qualified, we'll upgrade automatically
  example:loop().
```

The power of BEAM recursion is how the OTP framework functions. The OTP framework essentially defines different recursive infinite loops that handle (and categorizes) incoming messages. Those infinite loops will call code that developers write as needed. State is preserved through each iteration by passing it as a parameter to the next recursive call. State can then be "mutated" by simply passing a different value to the next recursion level. Recursion also is used to help facilitate hot code upgrades. In the end, this makes for a very powerful system.

## Messages, Actors, Modules, Behaviors

Erlang doesn't use classes, member functions, and inheritance. Instead, it uses actors, messages, modules, and behaviors.

Actors are simply code that sends and receives messages (often using a loop). Actors all run in isolated processes which minimizes impact of a failed actor and reduces the chance for memory-access data races. Erlang uses lightweight processes for each actor (kindof like green threads).

Messages are immutable data packets sent between actors. They don't execute immediately or cause the instruction pointer to jump to another part of the codebase - unlike method calls. Instead, messages are inserted into a message queue, and they are then sequentially processed by the actor. This helps remove a lot of race conditions without the need for developers to perform explicit locking or learning about borrowing semantics.

Modules are simply a group of functions that live in the same file. Some of the functions are exported, some functions remain private to that module. Modules can hold generic code that can be used anywhere (similar to utility classes). They can also hold the code for an actor. Often, actor modules will include both the actor code itself as well as the functions used to talk to the actor (it's public interface if you will). This helps with code locality since in a single file developers can see both the messages being sent and how messages are being processed. It also has helped me find several typos where my messages being sent and my messages being matched were different.

Behaviors are an abstraction around a common or generic code that can be used by modules (e.g. a recursive loop that receives messages). Behaviors are attached to a module, and a behavior will assume that the attached module will export specific functions. These exported functions act as the interface between the behavior and the module. Behaviors are similiar to traits.

Here's an example of everything.

```erlang
% Defines a module. This would be located in serve.erl
-module(serve).
% Declare we're using the "gen_server" behavior
% gen_server stands for "generic server" and is the basis for OTP servers/actors
% gen_servers basically receive messages and act on them
-behavior(gen_server).
% Export all of our functions
-export([start_link/1,hello/1,goodbye/1,init/1,handle_call/3,handle_cast/2,code_change/3]).

% Client functions (talk to actor)

% Starts an actor
% This is what calling code would use to talk to an actor made in this module
start_link(Args) ->
  % This both starts an actor and registers it under the id "serve"
  gen_server:start_link(?MODULE, ?MODULE, Args, []).

% Messages an actor

% sends the "hello" message
hello(Name) ->
  % gen_server:call is a request/response pattern
  % We send a request and then wait for a response
  gen_server:call(?MODULE, {hello, Name}).

% send the "goodbye" message
goodbye(Name) ->
  % gen_server:cast is a "fire and forget" pattern
  % We send a request but don't wait (or expect) a response
  gen_server:cast(?MODULE, {goodbye, Name}).

% Actor methods

% Initializes an actor; return value is a tuple of init status and initial state
init(_) ->
  {ok, []}.

% Handle synchronous (request/response) patterns
handle_call({hello, Name}, _From, State) ->
  io:format("Hello ~p~n", [Name]),
  % reply "{ok, ack}" and set state for next loop
  {reply, {ok, ack}, State}.;
handle_call(_Msg, _From, State) ->
  io:format("Unknown message received~n"),
  {reply, {error, bad_msg}, State}.

% Handle async (fire & forget) patterns
handle_cast({goodbye, Name}, State) ->
  io:format("Goodbye ~p~n", [Name]),
  {noreply, State};
handle_cast(_Msg, State) ->
  io:format("Unknown message received~n"),
  {noreply, State}.

% Handle code upgrades
code_change(_OldVsn, State, _Extra) ->
  % We aren't using state, so no need to change anything
  {ok, State}.
```

We could then use the above code like so:

```erlang
1> serve:start_link(). % Create an actor
{ok, #Pid<0.18.23>}
2> serve:hello("Bob"). % Send a message to the actor
Hello Bob              % Work done by the actor
{ok, ack}              % Response back from the actor
3> serve:goodbye("Bob"). % Send a message but don't wait for a response
ok
Goodbye Bob              % Work done by the actor
```

Here we have two separate actors, the REPL and `serve`. The REPL starts the `serve` actor and then sends the hello message to serve. The REPL then blocks, waiting for a reply. The serve actor will process it's queue, and when it gets to the REPL's message it will then print "Hello Bob" and send back an "ok" response. The REPL then sends a goodbye message and immediately returns. The serve actor will get to the message when it has time, and then it will print "Goodbye Bob".

### Mutability with Immutability

So far we've seen how to create an actor and how to do recursion. Now let's look at creating mutable systems with side effects.

Some functional languages, like Haskell, will require monads and monoids to encapuslate state. Some, like Clojure, will either defer to a non-functional language (e.g. Java) or it will have locking mechanisms around state (e.g. Clojure atoms).

In Erlang, recursion and actors are all that's needed to create mutability. Actors can "change" their state with recursion, and outside processes can send messages to tell the actor how to change state. Since messages are queued, outside processes don't have to worry about locking the actor to avoid a race condition. Actors are also able to send back responses, and depending on what other messages they've processed those responses can change. This then gives us a mutable system, even though the language itself is highly immutable.

To show this, we'll do a "button" increment/decrement example - except I'm going to use messages instead of actual buttons.

```erlang
-module(button).
-behavior(gen_server).
-export([start_link/1,increment/1,decrement/1,init/1,handle_call/3,handle_cast/2,code_change/3]).

% Client methods

start_link(_) ->
  % We aren't registering this module, so the caller will need to keep track of the process id (PID)
  gen_server:start_link(?MODULE, 0, []).

increment(ButtonId) ->
  gen_server:cast(ButtonId, increment).

decrement(ButtonId) ->
  gen_server:cast(ButtonId, decrement).

value(ButtonId) ->
  gen_server:call(ButtonId, value).

% Actor methods

init(Start) ->
  {ok, Start}.

handle_cast(increment, Count) ->
  {noreply, Count + 1};
handle_cast(decrement, Count) ->
  {noreply, Count - 1};
handle_cast(_, Count) ->
  {noreply, Count}.

handle_call(value, _From, Count) ->
  {reply, {ok, Count}, Count};
handle_call(_, _From, Count) ->
  {reply, {error, bad_msg}, Count}.

code_change(_OldVsn, _State, _Extra) ->
  % Reset to 0 on code change to mimic JS code changes ;)
  {ok, 0}.
```

We can use this as follows:

```erlang
1> {ok, Button} = button:start_link({}). % Start the actor
{ok, #Pid<0.12.34>}
2> button:value(Button).                % Initial value
{ok, 0}
3> button:increment(Button).             % State mutation
ok
4> button:increment(Button).
ok
5> button:value(Button).                 % New, mutated value
{ok, 2}
6> button:decrement(Button).             % More state mutation
ok
7> button:value(Button).                 % Current value
{ok, 1}
```

And viola! We have mutating state inside an immutable language!

Once I understood how state should be delegated to actors, things began to click a lot more. Before that realization, it felt like magic when I used a library or system which had "mutating" state. It also was frustrating to try to track state with only recursion. However, once it clicked that actors were the key, it felt like I had pulled away the curtain. "State" was really just an infinitely recursive loop running in it's own process. "Mutations" were really just making a recursive call inside an actor. That's all there was to it.

## Let it fail - but not really

Erlang's error handling most closely resembles Rust and Go, but with built in recovery mechanisms.

Go, Rust, and Erlang divide errors into two categories: expected and unexpected.

Expected errors are returned by value and should be handled by the developer. These are "forseable" circumstances. A great example of this is with File I/O. File I/O libraries are filled with tons of forseable errors, such as a file not being found, not having sufficient permissions, or mistaking a file for a directory. In these cases, the Erlang pattern is to use a tuple with the first element either being `ok` for success or `error` for an error. The second tuple element is either the result value or the error message. This allows developers to easily write error handling code and to know if a request could fail or not. Additionally, Erlang's pattern matching makes this really nice to work with.

Unexpected errors are similar to panics in Go and Rust. These are either conditions that the programmers believed were impossible (e.g. a bug, running out of disk space, out of memory, etc), or something catastrophic (e.g. a hardware failure). In Rust, when there's a panic the currently running thread will die. There isn't a recovery mechanism (yet). In Go, the panic can be "caught" through recover. However, if it's not caught then the program will crash. Any sort of recovery is either impossible or completely dependent on the developer (and often discouraged). This leads to a "defensive" coding style where developers do extra serialization and calls to avoid a panic crashing their code.

Erlang is different since it assumes that unexpected errors will happen, and it also assumes that a program must continue to function despite those errors. Erlang does provide similar "catching" mechanisms to Go's recover or Java's catch, but that's generally discouraged. Instead, Erlang pushes the philosophy that a crashed process should be recovered to a known good state (and possibly any processes depending on the crashed one). This is acheived through supervisors which watch each process, and when it sees a crash it will restart that process. Restarted processes will use the newest version of the code. They also don't maintain their old message queue or state, so it's more of a clean slate restart rather than a "recover from last known state" restart.

This is possible due to the built in "link" and "monitor" mechanisms of Erlang. When two processes form a link, their lifetimes become intertwined. If once process dies, the VM will either kill or notify the other process. This is bidirectional, so it doesn't matter which process dies, the other one will always be killed or notified. Monitoring is a one way connection. Process A can monitor process B, and if process B dies then process A will be notified. However, if process A dies then nothing will happen to process B.

Supervisors take advantage of these built in link and monitoring mechanisms to manage groups of processes. They essentially setup a link so that they get notified of any child process dying. Additinally, if the supervisor dies then the VM will kill the child processes. When a child dies, the supervisor will restart that process and increment a failure counter. If too many failures happend in a rolling timeframe, then the supervisor will terminate itself - thus escalating the issue to it's supervisor (if there is one).

Furthermore, supervisors can restart multiple processes on a single failure. This is important to avoid timeout issues from processes relying on a dead process. Supervisors can restart all of their children whenever one dies. Supervisors they can restart any children that were originally started after the dead child. Or they can restart only the dead child. These three restart strategies cover most use cases that I've come across, and anything that was slightly different was generally solved by restructuring which supervisors monitored which processes.

Overall, supervisors bring a lot to the table. Generally, when people talk about Erlang's "let it fail" slogan or how great the error handling is, they aren't talking about the error handling at all. Instead, they're talking about built in error recovery, which is a huge difference.

## Wrap Up

There's still a lot of Erlang left. I haven't talked much about the process registry, distributed Erlang, monitoring and observation systems, ecosystem, etc. That's mostly because I haven't used them much or haven't had very many "aha" moments. Sometimes I haven't found value from them - especially with distributed Erlang.

A few years ago I tried learning Erlang. Back then, I was learning it because I heard it was a distributed and scalable language. This was around the time people started promoting microservices, so I thought that Erlang would be the language of microservices. However, when I started learning it, I was rather disappointed. I didn't know enough about hostnames and `/etc/hosts` to get the distributed Erlang part working (for anyone struggling, you need to make sure that the remote node's hostname is added to your hosts file in addition to getting the cookie set). Additionally, actors and supervisors seemed like extra ceremony that didn't provide much value at the time. It felt really heavy weight for "micro" services, so I abandoned it and went with technologies like Docker, NodeJS, Spring Boot, and Clojure (and yes, I am aware of the irony that most of those are actually more heavy weight than Erlang - I've learned a lot since then).

At some point I started regaining interest in Erlang, so I started watching some talks and reading articles to see what I could learn. I encountered a lot about how WhatsApp architected their in-memory database with Erlang or how Discord used Elixir to massively scale to millions of concurrent users.

That was cool, but I didn't need that kind of massive scale. I've mostly worked in financial B2B SaaS software. The products I work on don't have massive amounts of concurrent users. A lot of our users will only login to our products a handful of times a month - assuming they login at all. A lot of the time our product was just an middleman between other software products or a background process that they checked in on as needed. Most of our scaling issues were in nightly runs, data imports and exports, report generations, and sending emails or text messages. And generally most of those scaling issues were solved by either using a third party service, sharding our databases by client, or spinning up extra EC2 instances at night. After seeing the numerous talks about how WhatsApp and Discord can handle more daily user interactions than what I get in a year, I felt that Erlang was only for people bigger than me with products more heavily used than mine. It was for services with massive amounts of concurrent load. I didn't need anywhere near that kind of scale. So I again stopped learning Erlang thinking that it was way more than I needed.

Fast forward several years, and I'm starting to learn Erlang again. However, this time it's not for scalability reasons (my needs haven't changed). Instead, it's for reliability reasons. I've seen a lot more production incidents. I've had to diagnose hard to track bugs where the server would cause mobile apps to crash. I've had to debug why an entire website was down, or why the site would crash for a subset of customers. I've dealt with bad data synchronizations across data centers, failed caches, backed up queues, servers running out of disk space, and more. Keeping systems running reliably is just hard.

At some point I decided to try to figure out how to make reliable systems. I started with Clojure since I thought that immutability plus some better null handling were key, and Clojure offerred both ([hence my last post](https://matthewtolman.com/article/2024-04-a-return-to-clojure)). Turns out, it wasn't enough by itself. I still had try/catches and null checks litter the code, and I still had the same amount of system crashes. I then rememberred reading or hearing about reliable systems written in Erlang. So I took a look, and was happily surprised (hence this post).

I have yet to make a realistic software codebase with Erlang (or Elixir which is built on Erlang). But, in my different test projects it looks extremely promising. I've been able to intentionally crash processes and things keep working. I've experienced the benefit of a supervision tree when I crashed the same process so many times it tripped the supervisor and caused that to crash as well. I've tried the different restart strategies. I also used the raw link and monitor capabilities to write some custom error recovery mechanisms. And yet in all of these tests my app keeps working. I've been very impressed by how hard it is to kill my small little test apps, even when using REPL commands to intentionally kill processes. So I am looking forward to writing and finishing a much bigger project and see how reliable I can make that

Erlang is less about making a distributed system and more about making a solid, reliable system. It's less about making the next WhatsApp or Discord and more about making a service that just works. It's less about spinning up many servers to create massive scale and more about having that one server running in a closet that no one thinks about because it doesn't have issues. It's about being able to avoid downtime (even planned downtime) without having massively complex Kubernetes setups and blue-green deployments. It's meant for small teams and projects which just want their code to work. That's the true beauty of Erlang, and I'm excited to keep using it.

