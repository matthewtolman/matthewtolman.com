This year I had made a goal to learn Go. I've had many starts and stops with learning Go, but I wanted to at least make a full program in Go this year (that wasn't from a tutorial).

I chose to rewrite one of my side projects, a [database migration tool](https://gitlab.com/mtolman/rove-migrate). I also wanted to extend the tool to have a "database driver over stdin/stdout," that way I could use a database driver in another language over with fork/pipe. 

I spent months learning Go, working with contexts, concurrency, databases, and more. I dove deep into the Go `database/sql` package and learned how to make my own SQL driver.

I was making a lot of progress. I was building software. I was making things. And then, I stopped. Why? Well, that's what this article is going to talk about.

## Why Go is Good

First, before we go anywhere, let's talk about why Go is a good language. It does many important things right, which is one of the reasons it's so popular.

### Lightweight Processes

Go has go-routines, which are small, lightweight, cooperative, and preemptive processes. When one go-routine blocks, the scheduler will swap it out for another one. If a go routine takes too long (around 10ms) then the scheduler will stop (preempt) it and let something else run. It's a good balance.

When Go first came out, I remember seeing go-routines and realizing that this was revolutionary. And it was. As Go grew, the concept of lightweight processes started spreading. Nowadays, it's less of a selling point, but it's still important enough it would be a disservice to not include it.

### Single, Statically Linked Executable Output

Single, statically linked executables are amazing. I don't think a lot of developers (especially in web) realize how hard it can be to distribute a program between systems. Most compilers come use shared linking by default which makes a lot of assumptions about how systems are configured. If a system isn't configured "just right," then the program won't run (or at least, not correctly).

To resolve the system configuration issues, developers will "package" their application (e.g. installer for Windows, .deb/.rpm/snap/flatpak/whatever-else for Linux, .dmg for Mac). Packaging must be done for every operating system, architecture, and distribution platform. This is a huge pain to do correctly.

Because of the headache of distributing binaries, a many developers will instead distribute tools using scripts (e.g. Python, JavaScript, etc) or binaries that require a separately installed runtime (e.g. Java). This puts less strain on the developer, but now adds more strain on the user. For instance, in order to use ESLint, I first need to install Node. But, because there's so many differnet versions of Node, I need to make sure I get the right one (e.g. the latest LTS). But, I also may have a tool that depends on an old version of Node and doesn't work on the latest, so now I need a way to separate them. I could use containers, a version manager, separate machines, etc. But that's the *user's* problem.

Go provides an alternative. Go compiles one statically linked file - meaning it doesn't make assumptions about how the system is configured. This means that a developer distributes a single file (per OS/architecture), and a user downloads that single file. No complex installers, no version managers, it's just one file. Way better.

### Cross Compilation

Native cross compilation is huge.

Getting a C/C++ for multiple platforms is a pain. To do so, you need a compiler toolchain for every OS, every architecture, and every language version you target. Sometimes an OS provider gives out a toolchain, but usually the toolchain requires an expensive license, buying the actual hardware, or is just garbage.

Next step is to look for toolchains someone else has made. There are some good ones, but usually they are outdated. Either they don't have the OS API that you need, or they don't support the standard version you need, or there's just some wonky bug you need to workaround. Or maybe it's not on your development platform (e.g. linux only or windows only).

If you can't find a toolchain that works, the next thing to try is to make one. How do you make a toolchain? Well, by getting a compiler, and then getting the system and standard libraries off of the target OS/CPU device, and packaging them together. How do you get the system and standard libraries? Usually by either downloading them from a random website that hasn't been DMCA'd yet, or by copying them off of the machine you're targeting. Which means you have to buy or borrow a machine.

Needless to say, I have ended up with an x86 Mac, ARM Mac, Raspberry Pi (for ARM Linux), system76 (for x86 Linux), and x86 Windows computer for cross-platform testing my C/C++ code. And that's just to test desktop platforms. At one point I had both an Android and iPhone so I could do mobile platforms as well, but it was too much so I don't test on mobile anymore.

Native cross compilation built into a language changes things a lot. It removes the need for multiple machines to build a program. It removes the issue of "well Clang on x86 Linux works, but Clang on MacOS doesn't, let me spend the next 6 hours figuring out which features Apple left out of their fork." Instead, everything just works. Native cross compilation is the biggest (and often only) reason I need to try out a new language.

### Errors as Values

Errors as values is amazing. It makes error handling clear. It makes it so programs are less likely to blow up unexpectedly (unlinke JavaScript). It shows where things could go wrong. It's great. Go did choose the most boring way to implement erros as values, but that's fine.

### Defer

Speaking of defer, the concept of defer is great. After learning about defer years ago when I first tried Go, I started adding it to my C++ code. It's not too hard to add a scoped defer to C++, [Ginger Bill has a great blog post about it](https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/). Scoped defers are awesome and amazing. They make doing anything with low-level C code so much easier! Especially when dealing with SDL or OpenGL. Introducing my own defer is one of the first things I do in any C++ project. It's just amazing! Sadly, Go doesn't use scoped defer, so when I talk about Go's defer statement, it won't be in this section.

### Channels (sort-of)

The concept of channels is really good. The implementation in Go is decent. I'll cover what I don't like in the next section, but here we'll talk about what I do like.

Channels are a great communication and synchronization primitive. They are also a great signaling tool. They turn the programming model from "get lock, read value, compute, write, unlock" to "wait for message, process message, send response." It's a lot more intuitive, and it's often the model I actually want. Go channels are decent, but not great. I'll talk more about them later.

### Productivity

Go is a great langauge if all you want to do is get something out the door. It feels like Node used to over 10 years ago. There's a strong enough community that most "how do I do that" questions can be answered with a library. And there isn't a package mess/crisis that Node currently has. The language is easy enough to work with that a small project can be made quickly. The community is healthy and growing. There's money flowing into the language. And most projects are being maintained. It's really nice to use.

I see why many people really like the language. It enables software to be made quickly. What's even better, it's been around a long time and has somehow avoided the chaotic mess that Node is in.

## What I don't like about Go

Now I've talked about the benefits of Go, so I'm going to talk about what I don't like.

### Panics and Recover

Errors as values are awesome. Recoverable panics are awful.

Go has both `panic` and `recover`. When Go code panics, it will call `defer` methods (and only defer methods) as it unwinds the stack. If a defer method calls `recover`, then Go will stop unwinding and continue execution.

If some cleanup code isn't in a `defer` (because defer is broken and you may be cleaning things up in a loop), then that clean up code will not run. So now, you have a resource leak.

This is a really bad design. I have had to work around and patch so many issues because of this design. And what's worse is if I use anybody else's code - including the standard library and including channels - then I have to deal with this. It's awful.

### Defer

Defer in Go sucks.

Yes, I love defer, but I only love *scoped* defer. Go doesn't have scoped defer, it has *function* defer, and that's a huge difference.

Scoped defer works inside of loops, so if I'm iterating over directories to open text files, then I can't rely on defer to close those files because otherwise I run out of resources. But, I also can't rely on closing the files inside the loop because of `panic` and `recover`!

Instead, I have to either wrap all my loop bodies with a separte function so I can call `defer` there, or I have to close my file handle twice - once in the loop and once in a defer. It's bad.

If you're a Go developer right now, and you are calling `defer` inside a loop body without wrapping it in a function, go fix your code right now. You probably haven't ran out of resources yet, but at some point you will, so fix your code before it happens.

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

Sure, I can simulate `errdefer`, but it'd be way better to just have it.

### Channels

The implementation of channels bugs me a lot. I really dread working with channels in Go. It takes so much effort to get things right, and I am never sure I've properly handled all the edge cases. I've ran into so many footguns it felt like channels were designed by the C++ comittee (though the syntax is good enough that I know it wasn't them). So, let's cover some of my issues with Go's channels.

### Unintuitive Deadlocking

I'm not opposed to deadlocking being possible or easy to do. It's possible in every langauge I've seen (though some require worse code than others). So long as it's reasonably quick to understand why a program deadlocks, I'm not opposed.

What I am opposed to is deadlocks being non-obvious to developers new to the language in simple examples. By "new to the language", I'm talking about developers who have been coding for a long time, who've experienced deadlocks, who are  learning Go, and are trying to use channels with minimal knowledge of how they work. These are Senior level developers who are exploring channels on their own. If they can't figure out why a simple program deadlocks, then it's unintuitive.

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

The above code deadlocks. Why? We'll, I'm trying to receive a value I haven't sent. If I've ever done request/response code, this makes sense. A server cannot receive a request that was never sent. So the solution then must be to swap the send and receive, right? 

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

Nope, that deadlocks too. And it's this specific "deadlock on send" behavior that I don't like about channels. The issue is with how channels are made in Go. They are made to block on *both* send and receive *by default*. To fix the above code, I have to add a new, magic number.

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

Now I can write one value, and it won't deadlock. But I can only write one value. If I try to write more, I deadlock again. For someone who is just being exposed to Go, it is unintuitive. For someone who knows Go really well, they have been burned enough times they just know better.

I understand the reasoning behind this decision (they want fixed-size channels instead of dynamically resizing channels). But, it's very developer unfreindly.

To get my point across even more, let's compare the send/receive deadlock against another language with a send/receive pattern: Erlang.

```erlang
main() ->
	% send
	self() ! 5,
	% receive
	receive
		I -> io:format("~p~n", [I])
	end.
```

In Erlang, this code just works. It sees the queue is too small, so it grows the queue on send. It then reads from that queue, and prints the message. This is good, intuitive behavior.

### Panic on close

If you close a channel in Go and send to it, you don't get an error. Instead, you get a panic! And that brings in a ton of issues around defer statements and recover.

I'm not opposed to channels erroring on send. If they returned an error then this section wouldn't exist, and I'd be happy. But they don't. They go into poorly defined control flow that can cause issues for anyone who isn't using `defer` because `defer` is broken and can cause resource exhaustion.

### Lack of Timeout on Receive

Erlang provides a beautiful way to add a timeout to receiving a response. They just add a special keyword to the receive statement.

```erlang
listener() ->
  receive
    msg -> process(msg)
  % Timeout after 5,000 ms
  after 5000 -> timeout()
  end.
```

Go has a similar structure to Erlang's `receive` called `select`. `select` will wait for any asynchronous channel activity (i.e. send/receive) to succeed, and then it will use that block of code. However, unlike Erlang there is no equivalent to `after`. Instead, you create a new "signal" channel and then listen to that channel to "timeout" or "cancel."

### Contexts

Contexts are an old problem with a new name. Instead of having Go developers creating channels for timeouts and task cancellation, now they create a Context which is itself a channel for timeouts and task cancellation (yes, I know values can be added to them, but really they're mostly used as a signal channel).

Granted, the name helps. But not much. I still have to select on a context, and I still have to pass the context. To give an idea of how intrusive this can be, here's an example of contexts in Go:

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

In contrast, Erlang has a completely different mechanic called "process linking." In Erlang, when a linked process terminates for an abnormal reason (basically any reason other than `normal`), then *all* linked processes will also terminate. This means I can replace the above Go monstrosity with the following Erlang code:

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

### Mixing Channels and Mutexes

Go has two synchronization primitives. They have both channels and mutexes. This means that not only do I have to think of the edge cases of channels, but I also have to think of edge cases of mutexes. And not only do I have to think of the edge cases of channels and mutexes, but I also have to think of edge cases for the *interactions* between channels and mutexes. What happens when a sender and receiver both try to lock a mutex? What if a mutex is hot and that slows down my receiver? How will that impact the sender? What if the sender blocks because the channel is full? Will that cause my mutex to deadlock?

It's just a lot of complexity for little gain. And while it's easy to brush it off with "well I'm only going to use channels in my code," in practice that doesn't work. Modern software development isn't about your code anymore. It's about how your code interacts with other people's code - and some of them prefer mutexes while others prefer channels. The interactions between the two will still happen, and you'll still have to deal with it.

### Monotony

Writing Go is monotonous. It's not because Go is bad, but because Go is really good at making product. It's super specialized for making product. To the point that anything which isn't geared solely to making product is probably not included.

When I am trying to make a product, Go is great. When I'm trying to experience a journey, Go is pretty bad. The programming experience of Go is not exciting, it's not desireable, it's not about the developer. Instead, The programming experience in Go is about delivery, hitting milestones, and getting something to the customer.

For a day job, I would love to use Go more. I don't get to use it at all where I'm at currently. However, for my "side projects" it's almost always has never been about delivery, it's always been about learning and experiencing the joy of coding. For me, writing code is the art, not the product at the end.

I wrote a lot of Go code when I had an end goal, a desired product to make (in my case, a database migration tool). Once that project was done and I didn't have any more ideas, I just moved on. I didn't stay with Go because it was a bad language. Despite all my gripes with it I believe Go is a fantastic language. I just hit a point where I was needing something focused more on the code writing experience rather than product delivery.

So, will I ever use Go again? Yes, of course I will. Go is a really good language and has some really good strengths. I have some project ideas and books that I want to start on, and Go will be the language of choice. For now I'm taking a break and just exploring.
