This year I had made a goal to learn Go. I've had many starts and stops with learning Go, but I wanted to at least make a full program in Go this year (that wasn't from a tutorial).

I chose to rewrite one of my side projects, a [database migration tool](https://gitlab.com/mtolman/rove-migrate). I also wanted to extend the tool to have a "database driver over stdin/stdout," that way I could use a database driver in another language over with fork/pipe. 

I spent months learning Go, working with contexts, concurrency, databases, and more. I dove deep into the Go `database/sql` package and learned how to make my own SQL driver.

I was making a lot of progress. I was building software. I was making things. And then, I stopped. Why? Well, that's what this article is going to talk about.

## Why Go is Good

First, before we go anywhere, let's talk about why Go is a good language. It does many important things right, which is one of the reasons it's so popular.

### Lightweight Processes

Go has go-routines, which are small, lightweight, cooperative, and preemptive processes. When one go-routine blocks, the scheduler will swap it out for another one. If a go routine takes too long (around 10ms) then the scheduler will stop (preempt) it and let something else run. It's a good balance.

When Go first came out, I remember seeing go-routines and realizing that this was revolutionary. And it was. As Go grew, the concept of lightweight processes started spreading. Nowadays, 3rd party libraries and extensions to standard libraries have been introduced in other languages bringing a similar feature. They do suffer from the fact that most legacy libraries which block don't play nice (common with old database drivers), but mini-ecosystems are being built around the new pattern so it's becoming less of an issue. Lightweight processes are now less of a selling point than they used to be, but it's still important enough, and it still works well enough in Go that it needs to be on the list.

### Single, Statically Linked Executable Output

Single, statically linked executables are amazing. I don't think a lot of developers (especially in web) realize how hard it can be to distribute a program between systems. Most compilers come use shared linking by default which makes a lot of assumptions about how systems are configured. If a system isn't configured "just right," then the program won't run (or at least, not correctly).

To resolve the system configuration issues, developers will "package" their application (e.g. installer for Windows, .deb/.rpm/snap/flatpak/whatever-else for Linux, .dmg for Mac). Packaging must be done for every operating system, architecture, and distribution platform. This is a huge pain to do correctly.

Because of the headache of distributing binaries, many developers will instead distribute tooling with scripts (e.g. Python, JavaScript, etc) or "binaries" that require a separately installed runtime (e.g. Java). This puts less strain on the developer, but now adds more strain on the user. For instance, Python's "http.server" module is a simple script that runs a static web server. I use it a lot. But, in order to use it I first have to install python, and then I have to download the module. I've seen this issue grow a lot with the NPM ecosystem. For instance, the other day I was trying to find a little CLI tool that just outputs open source license templates to a text file. Almost every tool I found required me to first install node and then use `npx` to run the tool (which also downloads the script and who knows what else). I've also had issues with almost every database migration toolchain requiring a different runtime to be installed (usually Jaava). While one new languagae isn't a big deal, before I know it I have ruby, perl, python, Java, Node, .Net Core, and Rust/Cargo just to _run_ the tools I use. I still have to install the tools in addition to installing those languages, and if I get the wrong version of a language (because for some reason language designers deside to make breaking changes to their language), I now have to spend hours figuring out the right version, resolving language version conflicts (usually by uninstalling the language and then installing a _language version manager_ to install the right version of the language), and so on.
It's one of the big reasons so many developers are trying to use containers and virtual machines right now to setup their _development_ environment. I don't want to install half a dozen languages or run virtual machines just to _develop_ code. I just want to install the handful of tools as binaries and _run them_. But, that's hard for the tooling developers to do, so they make it the _user's_ problem.

Go provides an alternative. Go compiles one statically linked file - meaning it doesn't make assumptions about how the system is configured. This means that a developer distributes a single file (per OS/architecture), and a user downloads that single file. No complex installers, no language version managers, no additional garbage bloat, no virtual machines, it's just one file. Way better. It's so much better, that for any development tooling I want to write in the future, Go will be my first pick (other options would include stable, statically linked, single executable, cross compiling languages, but nothing else I've found checks all of those boxes - usually they are too breaking change happy).

### Cross Compilation

Go has native cross compilation built in. Native cross compilation is huge.

Getting a C/C++ build for multiple platforms is a pain. To do so, you need a compiler toolchain for every OS, architecture, and every compiler. Each is incredibly important to test with since C++ code is not actually C++ code. Instead, C++ code is some compiler/platform specific variant of C++. Compilers don't agree on most things about C++. Intrinsics are obvious, but there's other issues such as sizes of types (32-bit and 64-bit pointers are different), feature completion (I still can't use some C++17 features on some compilers, even though they're introducing C++23 features to that compiler), feature implementation (especially around constexpr and templates, things vary just way too much in what's even compilable), and language syntax (most syntax differences falls in Microsoft vs GCC/Clang, but even Clang and GCC have differences). Plus, then there's all the Apple stuff because they forked Clang and somehow made it worse.

When compiling for a platform (OS/architecture/compiler combo), you need a toolchain for that platform. Sometimes an OS provider or hardware vendor gives you a toolchain, but usually the toolchain requires buying the platform and hardware targetted by that toolchain. This means that relying on vendor toolchains for cross compilation isn't possible.

So, the next step is to turn to the open source community and to look for toolchains someone else has made. There are some good ones, but usually they are outdated. Maybe they don't have the OS API that you need, or they don't support the standard version you need, or there's just some wonky bug in their toolchain that you need to workaround. Or maybe it's not available for your development platform/build server. I've had some success with open source community toolchains, especially with GCC when building on Linux. But it's been really bumpy, and often I'm still left with some platforms I cannot cross compile for (often anything Apple related).

If there's not an open source toolchain available, the next thing to try is to make one. How do you make a toolchain? Well, by getting a compiler, and then getting the system and standard libraries off of the target OS/CPU device, and packaging them together. How do you get the system and standard libraries? Usually by either downloading them from a random sketchy website, or by copying them off of the platform you're targeting, which means you have to buy or borrow a machine. I don't like the downloading option since I don't trust the website, and I don't have friends nearby who use the platforms I'm missing, so I'm left with just buying the platform.

Needless to say, I have ended up with an x86 Mac, ARM Mac, Raspberry Pi (for ARM Linux), x86 Linux, and x86 Windows computers for compiling and testing my C/C++ code. And that's just to test desktop platforms. At one point I had both an Android and iPhone so I could do mobile platforms as well, but it was too much so I don't target mobile anymore.

Native cross compilation built into a language changes everything. It removes the need for multiple machines to build a program. It removes the issue of "well Clang on x86 Linux works, but Clang on MacOS doesn't, let me spend the next 6 hours figuring out which features Apple left out of their fork." And most importantly, it removes the dumb compiler incompatibility problem plaguing C++ right now. Instead, everything just works. Native cross compilation is the biggest (and often only) reason I need to try out a new language. Go definitely hit this sweet spot.

### Errors as Values

Errors as values is amazing. It makes error handling clear. It makes it so programs are less likely to blow up unexpectedly (unlinke JavaScript). It shows where things could go wrong. It's great. Go did choose the most boring way to implement erros as values, but that's fine. It works. Not too much to say here.

### Defer

The concept of defer is great. After learning about defer years ago when I first tried Go, I started adding it to my C++ code. It's not too hard to add a scoped defer to C++, [Ginger Bill has a great blog post about it](https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/). I've ended up trying out a few different versions of C++ defer, and ended up settling on my own mash up between a few. C++ defers are all based on RAII which means they're scoped. And Scoped defers are awesome and amazing. They make doing anything with low-level C code so much easier! Especially when dealing with SDL or OpenGL. Introducing my own defer is one of the first things I do in any C++ project. It's just amazing!

Sadly, Go doesn't use scoped defer, it uses function level defer which is just so much worse. There are rarely any times I want a function level defer, and when I do it's almost always with error handling (i.e. clean up memory on error) and Zig's `errdefer` is so much better for that use case. It works well-enough in Go that it's good enough for most people until you use loops, which is actually why anonymous functions inside of loops is such a big pattern in Go (which I don't really like). It leads to some really weird looking code that's confusing for anyone used to scoped RAII, but as far as "enterprise" languages go it's a big step in the right direction. Most "enterprise" languages focused so much on "memory management" that they completely forgot about all the other system resources a program uses, like file handles, sockets, GPU, etc. They ended up adding in "final" and "using" which tends to work, so long as you don't use it in combination with too many newer concurrency models (e.g. JavaScript generators don't call final if you don't exhaust the generator). Defer is great because it's a unified way of handling all program resources.

### Channels (sort-of)

The concept of channels is really good. The implementation in Go is decent. I'll cover what I don't like in the next section, but here we'll talk about what I do like.

Channels are a great communication and synchronization primitive. They are also a great signaling tool. They turn the programming model from "get lock, read value, compute, write, unlock" to "wait for message, process message, send response." It's a lot more intuitive, and it's often the model I actually want. Go brings this model to the language as a primitive. This means it's a small, lightweight thing you can use in many different ways and as a general problem solving tool. Being a primitive though comes with it's own problems because it's a tool not a solution. I'll talk more about channels later, and the big reasons they aren't solutions.

### Productivity

Go is a great langauge if what you want to do is get something out the door. It feels like Node used to over 10 years ago. There's a strong enough community that most "how do I do that" questions can be answered with a library. And there isn't the package mess/crisis that Node currently has. The language is easy enough to work with that a small project can be made quickly. The community is healthy and growing. There's money flowing into the language. And most projects are being maintained. It's really nice to use.

It enables software to be made quickly. What's even better, it's been around a long time and has somehow avoided the chaotic mess that Node is in. Combined with it's other strengths, this makes Go ideal for getting a tool quickly made and shared with your team to solve problems and smooth out parts of the development process. It's definitely a language to keep in the toolbelt.

## What I don't like about Go

Now I've talked about the benefits of Go, so I'm going to talk about what I don't like.

### Panics and Recover

Errors as values are awesome. Recoverable panics are awful.

Go has both `panic` and `recover`. When Go code panics, it will call `defer` methods (and only defer methods) as it unwinds the stack. If a defer method calls `recover`, then Go will stop unwinding and continue execution.

If some cleanup code isn't in a `defer` (because Go's defer is broken and you may have cleaned up outside a defer as an inexperienced hack), then that clean up code will not run. So now, you have a resource leak, and it's one that's hard to find and you don't learn about until either you're really experienced with Go (aka. it's bitten you) or someone really experienced with Go tells you.

This is a really bad design. I have had to work around and patch so many issues because of this design. And what's worse is I cant avoid panic/recover. I can't say "well I'm going to ban this and I won't have to worry about it." Panic/recover are foundational to Go's error model. If I use anybody else's code - including the standard library and especially including channels - then I have to deal with this. It's awful.

I get why on the surface it sounds nice, especially since Initially, I thought it was much better than Rust's panic. The surface level argument is to keep a service running even when a new-to-the-language developer does something dumb, like unsafely unwraps an optional because that's what the crappy online tutorial showed them to do (a problem that's very much real in Rust's tutorial ecosystem). In this case, even though an inexperienced dev did something dumb, an experienced developer could "forsee" this and add a recover statement nto whatever higher level application framework (e.g. HTTP server framework) to prevent the application from crashing. That way a web companny can quickly spin up a bunch of new devs on Go, let them make mistakes that make it to prod, and avoid server crashes because the framework "recovers" and returns a 500. It sounds like a great idea, but the combination of a bad defer and a pattern that ignores non-deferred clean up code makes resource management a lot more complicated. I hate to say this, but when using Go I really miss `final` and `using` statements because I know that at least with those statements errors aren't going to cause resource leaks.

### Defer

Defer in Go sucks. It is fundamentally broken, the language designers can't fix it without breaking the world, and so we're just left with awful hacks.

I do love defer, but I only love *scoped* defer. Go doesn't have scoped defer, it has *function* defer, and that's a huge difference.

Scoped defer works inside of loops, so if I'm iterating over directories to open text files, then I can't rely on defer to close those files. Function defer doesn't work in loops because the end of a loop iteration isn't the end of a function. If I tried to use defer in a file iteration loop, I will run out of file handles (which has happened to me and is how I learned about the difference). But, I also can't rely on closing the files inside the loop because of the `panic` and `recover` nonsense discussed previously.

Instead, I have to either wrap all my loop bodies with a separte function so I can call `defer`, or I have to close my file handle twice - once in the loop and once in a defer. Generally, most people will use an anonymous function that they call as the loop body, which is already ugly and bad enough. However, until very recently, this was broken because Go didn't handle function closures properly, so instead of capturing the value of the iteration variable it would only capture the reference of the iteration variable, meaning that as the loop continued iterating you'd end up with a mess of hard-to-find bugs in your loops. They did finally fix this, but it was so bad. This was one of the biggest sticking points that drove me away previously. I just have been bewildered by how badly they got loops messed up. It felt like writing the `for` keyword was stepping on a landmine where I'd be spending the next 3 days trying to figure out why a loop doesn't loop.

If you're a Go developer right now, and you are calling `defer` inside a loop body without wrapping it in a function, go fix your code right now. You probably haven't ran out of resources yet, but at some point you will, so fix your code before it happens. If you are a Go developer and you're wrapping your loop body in a function and you don't know why, this is why. It's so you don't get paged at 2am when your server runs out of resources when somebody decides to add a `defer` to your loop.

Also, while we're on the topic of defer, it's a shame there isn't an "error defer" concept, like Zig's `errdefer`. Instead, you need named returns and a custom defer like the following:

```go
func mayError() (err Error, res int) {
  defer func() {
    if err != nil {
      // .. do error cleanup here
    }
  }()

  // Do code here
  if err = somethingMayFail() {
    return err, 0
  }

  return nil, 25
}
```

Sure, I can simulate Zig's `errdefer`, but it'd be way better to just have it.

### Channels

The implementation of Go channels bugs me a lot. I really dread working with channels in Go. It takes so much effort to get things right, and I am never sure I've properly handled all the edge cases. I've ran into so many footguns it felt like channels were designed by the C++ comittee. So, let's cover some of my issues with Go's channels.

### Unintuitive Deadlocking

I'm not opposed to deadlocking being possible or easy to do. It's possible in every langauge I've seen (though some require worse code than others). So long as it's reasonably quick to understand why a program deadlocks in a simple case, I give a languages a pass. I'm not going to hold long debugging sessions of really bad or overly complicated code against a language - that's not the language's fault.

What I am opposed to is deadlocks being non-obvious to developers new to the language in simple examples. By "new to the language", I'm talking about developers who have been coding for a long time, who've experienced deadlocks, who are learning Go, and are trying to use channels with minimal knowledge of how they work. These are Senior level or higher developers who are exploring channels on their own. If they can't figure out why a simple program deadlocks, then it's unintuitive.

Let's start off by giving an intuitive example of a deadlock.

```go
func main() {
  ch := make(chan int)
  // receive
  res := <-ch
  // send
  ch <- 5
  print(res)
}
```

The above code deadlocks. Why? We'll, I'm trying to receive a value I haven't sent. If I've ever done request/response code, this makes sense. A server cannot receive a request that was never sent. A message broker cannot broadcast a signal that doesn't exist yet. So the solution then must be to swap the send and receive, right? 

```go
func main() {
  ch := make(chan int)
  // send
  ch <- 5
  // receive
  res := <-ch
  print(res)
}
```

Nope, that deadlocks too. And it's this specific "deadlock on send" behavior is what I don't like about channels. The issue stems from how channels are made in Go. They are made to not dynamically allocate more memory, and to guarantee no message is lost. They do this by having a synchronization point where both a sender and a receiver must be ready before a message is able to be exchanged. That way they never have to choose between either allocating memory or dropping a message if a receiver is not ready yet. The consequence is that channels block on *both* send and receive *by default*. To fix the above code, I have to add a new, magic number to tell Go that I want a fixed size buffer for messages to wait in case the receiver isn't ready.

```go
func main() {
  ch := make(chan int, 1)
  // send
  ch <- 5
  // receive
  res := <-ch
  print(res)
}
```

Now I can write one value, and it won't deadlock. But I can only write one value. If I try to write more, I deadlock again.

```go
func main() {
  ch := make(chan int, 1)
  // send
  ch <- 5
  // second send deadlocks
  ch <- 4
  // receive
  res := <-ch
  print(res)
}
```

For someone who is just being exposed to Go, this is unintuitive and takes quite a while to understand. It's one of those "you don't understand until you've been burned" quirks about Go. I understand the reasoning behind this decision (they want fixed-size channels instead of dynamically resizing channels). But, it's very developer unfreindly and very prone to bugs.

To get my point across even more, let's compare the above send/receive code against another language with a send/receive pattern: Erlang. Here's the single send/receive code rewritten in Erlang

```erlang
main() ->
	% send
	self() ! 5,
	% receive
	receive
		I -> io:format("~p~n", [I])
	end.
```

In Erlang, this code just works. Erlang will grow message queues as needed on send, so it doesn't block the send statement. Since the send statement doesn't block, Erlang moves onto the receive statement which reads from that message queu and prints the message.This is good, intuitive behavior.

### Panic on channel close

Go's bad behavior around sending messages doesn't just end with blocking. If you close a channel in Go and send to it, you don't get an error. Instead, you get a panic! And that brings in a ton of issues around defer statements and recover. This gets worse when you want to safely handle a "channel closed by receiver case." Because panics immediately unwind the stack, simply recovering in a defer won't resume the function. Instead, it returns back to the caller of the function. This means that in order to safely handle a send to a potentially closed channel, you must wrap that send in it's own function. This gets even more problematic when you're doing multiple safe sends as part of a select statement. Currently, I don't know if any of my solutions properly handle all edge cases, so I'm not going to share them here.

I'm not opposed to channels erroring on send. If they returned an error then this section wouldn't exist, and I'd be happy. But that's not how they work. Instead, channels go into poorly defined control flow that can cause issues for anyone who isn't using `defer` because `defer` is broken and can cause resource exhaustion. I also don't know why they didn't do it. The only thing I could think of would be they didn't want a variable assignment on send inside a select statement, but they already have multiple variable assignments on receive inside select statements so it's not a major syntax change. Sending to a closed channel just seems like a normal error case, it doesn't warrant destroying an entire application stack and risking resource leaks to me.

### Lack of Timeout on Receive

Go doesn't provide an easy way to do timeouts on receive. Yes, there technically is a way (I talk about it next), but it's not intuitive, it's really cumbersome, and it comes with a lot of "trust me bro" vibes which I don't like. Here's Go's solution:

```go
func listener(channel: <-chan int) {
  cancel, timeoutCtx := context.withTimeout(context.Background(), 5000*time.Millisecond)
  select {
    case in <-channel: {
      process(in)
    }
    case <-timeoutCtx.done(): {
      timeout()
    }
  }
}
```

In contrast, Erlang provides a beautiful way to add a timeout to receiving a response. They just add a special case to the receive statement.

```erlang
listener() ->
  receive
    msg -> process(msg)
  % Timeout after 5,000 ms
  after 5000 -> timeout()
  end.
```

Go's `select` is fairly similar has a similar to Erlang's `receive`, so it shouldn't be too much work to add a separate "after" statement to it. Doing so would make the whole situation so much easier and less cumbersome, especially since timeouts are super important to make sure code works properly. I don't think that Go will ever do that though since they seem to be of the philosophy "if we have a solution, don't add a new one even if the current solution really, really sucks." I do get that to some point, they're trying really hard to avoid becoming C++26 or JavaScript 2025, but they also do seem to be a bit overzealous with that philosophy.

### Contexts

Contexts are an old problem with a new name. Before contexts, Go developers would create signal channels and listen to those channels to know if a task should be cancelled (e.g. it timed out). Now, instead of having Go developers creating signal channels, they instead create contexts which are those signal channels. They did add a few additional methods that help in properly setting up the signal channels, and they did add a hash map so you can "store additional information", but under the hood it's basically a signal channel.

This helps a little, but not much - especially since a lot of tutorials and documentation are now "genericised" so it's really hard for someone new to the language to understand what's going on or if contexts are needed. Once you understand they're just signal channels, it's pretty easy to understand. The issue is that everyone will just go "no, actually they're much better than signals" which just makes it more confusing to new develoeprs. I still have to select on a context, and I still have to pass the context, and it's just as intrusive to use as signals. Here's an example of contexts in Go:

```go
func doWork(input <-chan int, output chan<- int, ctx Context) {
  for {
    select {
    case v, ok := <- input:
	if ok {
	  res := doSomething(v)
	  select {
	    case output <- res: continue
	    case <-ctx.done: return
	  }
	} else {
	  return
	}
      case <-ctx.done: return
    }
  }
}

func task(output chan<- int, ctx Context) {
  items := get_items()
  ch_in := make(chan int, len(items))
 
  go func() {
    doWork(ch_in, output, ctx)
  }()

  for _, item := range items {
    ch_in <- item
  }

  close(ch_in)
}

func launch_task(output chan<- int) (Context, CancelFunc) {
  ctx, cancel := WithCancel(Background())
  go task(output, ctx)
  return ctx, cancel
}
```

It's a lot of code to simply cancel a task. It's really inelligant, tedious, verbose, and easy to get wrong. It's the type of thing I'd expect some sort of macro system to wrap and everyone just uses the macro, but Go doesn't have macros so we're left with copy/paste. It's also easy enough to get slightly wrong that I don't trust AI autocomplete to do it correctly every time.

I really do prefer Erlang's solution to the same problem of task cancellation. Erlang has a mechanic called "process linking." When a linked process terminates for an abnormal reason (basically any reason other than `normal`), then *all* linked processes will also terminate. This means I can replace the above Go monstrosity with the following Erlang code:

```erlang
do_work([Work | Rest], ParentPid, OutputPid) ->
  OutputPid <- do_something(Work),
  do_work(Rest, ParentPid);
do_work([], ParentPid) ->
  ParentPid <- done.

task(OutputPid) ->
  % Spawn work and link to it
  spawn_link(?MODULE, work, [get_items(), self(), OutputPid]),

  % Do parallel work here

  receive
    % wait for child process
    done -> ok
  end.

launch_task(OutputPid) -> spawn(?MODULE, task, OutputPid).
cancel_task(TaskPid) -> exit(TaskPid, cancel).
```

The fact that linking doesn't end child processes on `normal` execution is a bit weird and can cause some problems, but the reason for it is since linking is two-way. If the child process dies for an abnormal reason, it takes down the parent process too, which is useful if say a worker process crashes that way the parent process doesn't hang forever. By having `normal` execution not crash everything, it allows a child process to complete whatever it needs to do without taking the system down. My preference would be for one-way linking to be the norm, and to have an option for a one way link to terminate on every end condition (e.g. parent finishes and automatically cleans up children) or only on errors (e.g. child crashes and takes down subsystem). In short, Erlang's solution isn't perfect, but it's a lot better than Go's solution.

### Mixing Channels and Mutexes

Go has two synchronization primitives, channels and mutexes. This means that not only do I have to think of the edge cases of channels, but I also have to think of edge cases of mutexes. And not only do I have to think of the edge cases of channels and mutexes, but I also have to think of edge cases for the *interactions* between channels and mutexes. What happens when a sender and receiver both try to lock a mutex? What if a mutex is hot and that slows down my receiver? What if the sender blocks because the channel is full? Will either of those cases cause my mutex to deadlock?

It's just a lot of complexity for little gain. And while it's easy to brush it off with "well I'm only going to use channels in my code," in practice that doesn't work for very long. This has the same problem as the whole `panic` and `recover` issue. Modern software development isn't about your code anymore. It's about how your code interacts with other people's code - and some of them prefer mutexes while others prefer channels. The interactions between the two will still happen, and you'll still have to deal with it.

### Monotony

Writing Go is monotonous. It's not because Go is bad, but because Go is really good at making product. It's super specialized for making product. To the point that anything which isn't geared solely to making product is probably not included. This includes things like advanced type systems, compile time execution, etc.

When I am trying to make a product like a development tool, Go is great. When I'm trying to experience a journey, Go is pretty bad. The programming experience of Go is not exciting, it's not desireable, it's not about the developer. Instead, The programming experience in Go is about delivery, hitting milestones, and getting something to the customer.

For a day job, I would love to use Go more. I don't get to use it at all where I'm at currently.

However, for my "side projects" it's almost always has never been about delivery, it's always been about learning and experiencing the joy of coding. I want a language that provides a lot of interesting stuff to bite into, especially with compile time execution. Go doesn't provide that sort of intrigue, so once I hit my end goal of a finished project, I jsut moved on.

So, will I ever use Go again? Yes, of course I will. Go is a really good language and has some really good strengths. I have some project ideas and books that I want to start on, and Go will be the language of choice for those. But for now I'm currently exploring something else.
