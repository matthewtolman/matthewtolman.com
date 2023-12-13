
I've been working on my Test Jam project for a while. I had previously tried to do a WASM wrapper around my C/C++ build, but I ran into a memory leak which existed outside the C/C++ code. Instead of reading the manual, I rewrote the code in TypeScript. After the rewrite, I read the manual and learned what was the source of the leak. I then rebuilt my WASM wrapper and ended up with a TypeScript and WASM implementation. I then decided to start comparing the two implementations to see what the benefits and drawbacks are. I also wanted to test some well touted claims about WASM (e.g. performance, code reuse) in the context of a library. So I started poking and prodding, and then I created this post.

[toc]

## Performance

We'll start off with the typical performance stuff first. For this test, I'm going to test several methods: one which returns a random number, one which returns a random ASCII string (with any ASCII character), and one which returns a random URL. These three methods are meant to show off a few different aspects.

> For my setup, I'm using Node v18.12.1 on an Intel MacBook (2.6 GHz 6-Core Intel Core i7) with 16 GB of RAM (DDR4) running MacOS Ventura 13.1. I'm also running a few background programs (such as CLion, Firefox, etc). I'm less interested in "small performance differences" and more interested in orders of magnitude of performance gains. If anything is somewhat close, I'll call it a draw (we'll see a few). 
> 
> For compiling, I'm using Emscripten 3.1.48 building with CMake 3.27.6 with the `CMAKE_BUILD_TYPE` set to Release and the toolchain set to Emscripten's CMake toolchain file. I'm also creating a single JS file with `-sSINGLE_FILE`, and I'm disabling async WASM loading with `-sBINARYEN_ASYNC_COMPILATION=0`.

### Ideal Case: Numbers

The random number method is the ideal WASM use case. WASM works really well when transferring numbers that can be converted into a 64-bit floating point number. There's littler overhead, and it's supposed to be fast. I'm using the [TinyMT](http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/TINYMT/) algorithm for both, with the native C implementation for WASM and my own port for TypeScript. 

> For all benchmarks, we're measuring in terms of operations per second. The higher the number the better. Also, TypeScript will always be on the left and WASM will always be on the right.

Here's the results:

~img::src=/imgs/creating-a-wasm-library-with-embind/number-bench.png::alt=Bar chart depiciting TypeScript with 500,000 operations per second and WASM having 6,500,000 operations per second:;

That's quite the difference. WASM has 6.5 million operations per second while TypeScript has half a million operations per second. This is the typical clickbait "WASM go brrr" graph. This is also the best case for WASM where overhead is minimal. So, let's look at something that's more of a fair fight.

When WASM returns data, it returns raw memory, and the host code (in this case JavaScript) must convert that memory into something useful, and then it must tell WASM "hey I'm done with the memory, clean it up please". In our case, we're just returning a double, which JavaScript just copies into a double, so barely any work was done.

But what if we returned a string?

### Returning random strings

Specifically, we're returning a C++ `std::string` which uses 8-bit characters. Which means we're either using ASCII strings or UTF-8 strings. However, JavaScript uses [UTF-16 strings](https://tc39.es/ecma262/multipage/ecmascript-data-types-and-values.html#sec-ecmascript-language-types-string-type). This means that we need to convert our 8-bit C++ strings to JavaScript's 16-bit strings. Emscripten does this for us automatically (it assumes UTF-8), but it is a step which happens.

> For you Rustaceans, you're going to run into similar issues. Rust uses [UTF-8 strings](https://doc.rust-lang.org/rust-by-example/std/str.html), which also need to be converted to UTF-16, so you'll be taking this performance hit every time you return a Rust string.
>
> Also, I do know that C++ has `std::wstring` which uses 16-bit characters. I'm used to Unix programming where I don't have to deal with Microsoft's wide character stuff. I just work in UTF-8 since all the functions I care about take either `const char*` or `const std::string&`. 

To measure that overhead, I chose to measure my simplest string generation code. All it does create a random string from random ASCII characters (0-127). I kept the same random number generation library to limit how many variables are being changed, so we still will have the above difference underneath. Here's the benchmark:

~img::src=/imgs/creating-a-wasm-library-with-embind/ascii-any.png::alt=Bar chart depiciting TypeScript with 43,000 operations per second and WASM having 45,000 operations per second:;

This benchmark is a draw. But, the benchmark isn't an "apples to apples" comparison. It turns out that V8 [has a trick to achieve much faster string concatenation](https://github.com/dart-lang/sdk/issues/1216#issuecomment-108307883), but at the cost of actually reading the string contents. Meanwhile, Emscripten is just building the string in it's entirety instead of using the concatenation tricks.

We can see the difference if we actually read the string. I chose to do that by using a TextEncoder to encode both strings. Here's the adjusted results:

~img::src=/imgs/creating-a-wasm-library-with-embind/ascii-any-encode.png::alt=Bar chart depiciting TypeScript with 55,000 operations per second and WASM having 65,000 operations per second:;

The WASM code is a bit faster (about 10-20%), but it's not a mind blowing amount. It's not a large enough amount I would consider WASM to be a clear choice. It's close enough that for many applications TypeScript is fast enough.

It's also not necessarily a typical comparison. Generally I would have implemented my WASM code using `std::stringstream`, but I wanted to push myself so I used preallocated buffers and had optimizations to lower heap allocations wherever I could. If I switch to `std::stringstream` (how I'd normally write it), we'd get the following graph:

~img::src=/imgs/creating-a-wasm-library-with-embind/ascii-any-sstream.png::alt=Bar chart depiciting TypeScript with 60,000 operations per second and WASM having 25,000 operations per second:;

All this to say that there isn't a clear winner. Yes, WASM code *can* be faster, but it is not automatically faster. V8 has done a lot of JIT optimizations around the typical use case, whereas WASM requires developers to really know their tools, libraries, and language to match the performance.

### Returning Random URLs

Now onto our final benchmark, the URL generation. My random ASCII string code is just a simple loop, so it's pretty easy for V8 to optimize. I wanted to see how well JavaScript and WASM compared when there was a moderate amount of complexity. Here are the results:

~img::src=/imgs/creating-a-wasm-library-with-embind/url-gen.png::alt=Bar chart depiciting TypeScript with 65,000 operations per second and WASM having 145,000 operations per second:;

Performance is a bit of a mixed bag. WASM can be faster if it is properly written and there is minimal overhead in translating between WASM and the host language, but it can also be slower (sometimes by a significant amount). Performance is definitely a "it depends", which isn't great if that's the main selling point. So let's start looking at a few different attributes of WASM and see the good and bad.

## Memory Management

One of the benefits of WASM is the ability to reuse native code. Which sounds great, but there is an issue with that. WASM has a memory model that doesn't play nice with JavaScript, so reusing native code through WASM can be difficult.

WASM code is given a chunk of memory, and then it is up to the WASM developer to decide how to manage that memory. When WASM returns a value to JavaScript, what it returns is a reference to a chunk of memory. WASM code then needs to keep that memory address valid so long as JavaScript is using it. In C/C++, this is done by not calling `free`, and in Rust it's done by turning off the borrow checker (e.g. via `unsafe`). In return, JavaScript will let WASM know when it's done with that memory, that way WASM can free it. In Embind this is done by calling a `delete` method.

This sounds simple, until we realize that this is manual memory management in a garbage collected language. What's more, this is manual memory management inside of JavaScript - a language known for weird quirks and behaviors. So, let's look at some different strategies for dealing with WASM memory, and the quirks to look out for.

### Strategy 1: Manual Memory Management

The first approach is for the JavaScript developer to use their inner C developer and clean up the memory manually. Because JavaScript can throw (and throws are non-obvious), all `delete` calls should be done in a `try/finally` like so:

```javascript
const wasmObj = new myWasmModule.MyCppClass()
try {
	/// Do something with wasmObj ...
}
finally {
	wasmObj.delete()
}
```

The biggest gotchas with this approach are async code (Promises, callbacks, etc) and generator functions.

Consider the following async code:

```javascript
const textProcessor = new myWasmModule.TextProcessor()
try {
	const results = await Promise.all([
		fetch(myUrl1).then(res => res.text())
		fetch(myUrl2).then(res => res.text())
	])
	return results.map(res => textProcessor.process(res))
}
finally {
	textProcessor.delete()
}
```

If one of the fetch calls hangs, then the WASM memory will stay around indefinitely. This is true of any async operation. The solution is to not allocate memory before queuing an async operation. Instead, allocations are only done inside the synchronous step and then are immediately deallocated, like so:

```javascript
const results = await Promise.all([
	fetch(myUrl1).then(res => res.text())
	fetch(myUrl2).then(res => res.text())
])
return results.map(res => {
	const textProcessor = new myWasmModule.TextProcessor()
	try {
		textProcessor.process(res)
	}
	finally {
		textProcessor.delete()
	}
})
```

Alternatively, all async operations could have timeouts and limits to make sure that they are guaranteed to complete in a reasonable amount of time. Just be careful to not create too many memory allocations at once - otherwise WASM will run out of memory.

Generator functions also add a few additional wrenches into the mix. Consider the following code:

```javascript
funciton* myGen() {
	const queue = new wasm.QueueOfRandomNumbers()
	try {
		for (; !seq.empty(); queue.pop()) {
			yield queue.front()
		}
	}
	finally {
		queue.delete()
	}
}

function first(seq) {
	for (const v of seq) {
		return v
	}
}

console.log(first(myGen()))
```

It looks correct (we're wrapping with a `try/finally`), but the above code will leak memory. The reason is that generators suspend execution whenever they yield. Code inside a `finally` block is only called when the generator finishes with a `return` or a `throw`. It is never called on a `yield`.

Since our generator only reaches a `yield` and never finishes, our `finally` block never runs and we end up leaking memory.

We have to be extremely careful with how and where we clean up memory. Manually freeing is a style some C developers may like, but most JavaScript developers want something a little more automated. So let's look at something else. 

### Strategy 2: (Not Using) FinalizationRegistry

Having a way to clean up WASM memory when the JavaScript garbage collector runs would be nice. One way to (theoretically) do this would be to use the `FinalizationRegistry`. The idea behind the `FinalizationRegistry` is that you can register a method to run when an object is cleaned up, that way we could perform extra cleanup steps. Some example code is below:

```cpp
// C++ Code
#include <string>
#include <cstdio>
#include <emscripten/bind.h>

using namespace emscripten;

struct MyString {
	std::string value;
	MyString() : value(99999UL, 'a') {
		printf('initializing\n');
	}
	~MyString() {
		printf('cleaning up\n');
	}
};

EMSCRIPTEN_BINDINGS(Demo) {
	class_<MyString>("MyString")
		.constructor()
		.property('value', &MyString::value);
}
```

```javascript
// JavaScript code
const mod = require('./cpp-wasm-output')
const maxIter = 3
const wasmRegistry = new FinalizationRegistry((val) => { val.delete() })

function str() { return {str: new mod.MyString()} }

for (let i = 0; i < maxIter; ++i) {
	const val = str()
	wasmRegistry.register(val, val.str)
}
```

Looks great! Let's run it. Below is my output:

```text
initializing
initializing
initializing
cleaning up
cleaning up
```

Yes, there are only two `cleaning up` prints when there should be three. 

This is because the `FinalizationRegistry` feature is broken. Per the spec, the `FinalizationRegistry` code is only a suggestion. A JavaScript engine can completely ignore `FinalizationRegistry` and [still be standards compliant](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/FinalizationRegistry#notes_on_cleanup_callbacks). It's frustrating that this feature is so promising on the surface, but so terrible in reality. Fortunately, there are other ways.

### Strategy 3: Custom Garbage Collector

Instead of relying on the built-in garbage collector, let's make our own. [Bram Wasti gives details how to create a custom garbage collector for WASM](https://jott.live/markdown/js_gc_in_wasm). Essentially, instead of using `FinalizationRegistry`, we use `WeakRefs`. `WeakRefs` are references to objects in JavaScript memory. When the garbage collector clean up the referenced objects, it will also update the associated `WeakRefs` to point to nothing. This allows us to detect if an object is cleaned up ourselves.

Fortunately, `WeakRefs` are a well defined part of the standard, so they won't be blatently ignored

Here is my JavaScript code:

```javascript
const mod = require('./cpp-wasm-output')

// used to allow terminating a node process
// not required for processes which never end (e.g. server, browser)
let stopGc = false

// tracks WASM allocations
const managed = {};

(async function garbageCollector() {
	const forceCleanup = (key, cleanup) => {
		cleanup()
		delete managed[key]
	}
	const tryCleanup = ([key, [ptr, cleanup]]) => {
		if (!ptr.deref()) {
			forceCleanup(key, cleanup)
		}
	}

	// Main GC loop
	while (!stopGc) {
		Object.entries(managed).forEach(tryCleanup)
		await new Promise(res => setTimeout(res, 10))
	}
	// Exiting program, ensure proper cleanup
	Object.entries(managed).forEach(forceCleanup)
})();

// For production, replace with better ID generation
let id = 0
function manage(obj, cleanup) {
	managed[id++] = [new WeakRef(obj), cleanup]
}

// Wrap WASM class
class MyString {
	constructor() {
		// Create our WASM object
		const data = new mod.MyString()
		// Add to our custom GC tracker
		manage(this, () => data.delete())
		// Add as a class property
		this.data = data
	}

	str() { return this.data.value }
}

// Do our iterations
async funciton f() {
	const maxIters = 25
	for (let i = 0; i < maxIters; ++i) {
		const v = new MyString()
		// Allocate a lot of memory to create memory pressure
		// Without memory pressure, the GC won't update WeakRef's
		Array.from({length: 50000}, () => () => {})
		// Also, since our GC is on a timer we need to wait
		await new Promise(res => setTimeout(res, 100))
	}
}

// Call f() and then tell the GC to end the program
f().then(() => stopGc = true)
```

There is still wonky behavior due to how V8's garbage collector works. Some of the time it cleaned up references mid-run, other times it cleaned up at the end.

One thing to keep in mind is that this strategy doesn't know how much remaining memory WASM code has. It relies on the JavaScript garbage collector which only sees the JavaScript memory that needs to be deallocated. It's quite possible to have way more WASM allocations than JavaScript allocations. When that happens, you may still run out of WASM memory because the JavaScript garbage collector may never run. Fortunately, WASM code still isn't very popular, so most JavaScript applications shouldn't run into that issue.

### Strategy 4: Copy into JavaScript

Another way to deal with WASM memory is to copy the data into JavaScript objects and immediately free the WASM memory. It does take writing some wrapper code to do the copying, but the benefit is that the rest of the code is dealing with JavaScript objects like normal.

> This is the memory management strategy I used for my library. My library's purpose was mostly to create random values and not have a back-and-forth between JavaScript and WASM.

The code would look something like the following:

```javascript
const mod = require('./cpp-wasm-output')

class MyString {
	constructor() {
		const data = new mod.MyString()
		try {
			// Embind converted data.value to a JS string
			// so we don't have to deep copy the string
			this.data = data.value
		}
		finally {
			// Immediately clean up our WASM memory
			data.delete()
		}
	}
}

const maxIters = 25
for (let i = 0; i < maxIters; ++i) {
	const v = new MyString()
}
```

The downside is that it doesn't work well with code that has a back-and-forth, such as classes with methods.

```cpp
class Person {
public:
	Person(int id);

	auto get_name() -> std::string;
	auto get_age() -> int;
	auto is_adult() -> bool;
private:
	int id;
	std::string name;
	int age;
}

EMSCRIPTEN_BINDINGS(Demo) {
	class_<Person>("Person")
		.constructor<int>()
		.method('getName', &Person::get_name)
		.method('getAge', &Person::get_age)
		.method('isAdult', &Person::is_adult);
}
```

I have yet to find a perfect solution for WASM memory. There are some drawbacks to all of them. The custom garbage collector is pretty nice and would work for many use cases. To be truly universal it would need some work. For instance, we need a way for a failed WASM allocation to trigger a JavaScript garbage collector cleanup and try again.

## Lines of Code

One of the promises of WASM is that it will let us share code between backend and frontend (assuming your backend is written in a systems language). Which sounds great because that should mean less code on the frontend. So, I put it to the test and decided to measure the lines of code in my project.

> Do keep in mind that I'm copying my WASM objects into JavaScript objects, so I have quite the wrapping layer. I may have been able to cut down on lines of code with a different memory management technique, but since I haven't tried I don't know.

For my lines of code calculations I used `cloc`. I also only measured the generator code since that's the only part of my library which used WASM (the rest of the code was just copy/pasted files, so no saving lines there). For the C++ column, I'm measuring the size of my `bindings.cpp` file (the file which defines my Embind bindings to JS). That file is necessary for the WASM compilation, so I'm counting it. Here's the results:

| TypeScript | WASM (TypeScript) | WASM (C++) | WASM (Total) |
|----|----|----|----|
| 1,413 | 1,241 | 442 | 1,683 |

I only saved 172 lines of TypeScript code. What's worse is that I had to write way more C++ code than what I saved in TypeScript code, so my WASM implementation is actually 270 lines longer.

The benefits of "code reuse" are a bit muddied now. On the one hand, I'm saving lines of code by porting over. I'm also simplifying my build pipeline and avoiding a lot of the memory headaches discussed above. I also get to take full advantage of the language I'm porting to.

But on the other hand, I'm replicating logic which means I need to keep my changes "in sync." I'm also having to deal translate between how languages believe code should be written. For instance, in C++ I have unsigned integers which makes using the TinyMT algorithm easy. In JavaScript I only have doubles, so [I had to emulate unsigned arithmetic](https://matthewtolman.com/article/unsigned-integers-in-javascript). This type of gotcha can make porting code difficult, and it can make maintaining changes even harder.

The other issue is that this is one data point with one memory management solution. So it's extremely hard to extrapolate to other projects.

For the "code reuse" benefit, I'm going to say it's shaky. I wouldn't ever propose WASM on only the grounds of "code reuse" - especially since using the WASM code is pretty hard.

## Bundle Size

My next test was to take a look at bundle sizes. This is again a single data point, so don't extrapolate on the actual sizes (is it bigger or smaller). I'm looking at how well does the WASM code compress, not the actual size of the code.

> As a reminder, I'm using Emscripten to create the WASM output. Emscripten is known for filling in compatibility gaps (e.g. convert OpenGL to WebGL, provide POSIX polyfills, emulate pthreads, etc), not small bundle sizes. So do keep in mind that another compiler or toolchain could lower the WASM sizes dramatically (e.g. [Cheerp](https://labs.leaningtech.com/cheerp), [Zig](https://ziglang.org/), [WASI SDK](https://github.com/WebAssembly/wasi-sdk), [LLVM](https://surma.dev/things/c-to-webassembly/), etc).

For bundling, I used esbuild to create a single JS bundle for both my TypeScript and WASM implementations. Here's my sizes (no minification or compression):

```text
TypeScript: 886.4kb
WASM:       694.7kb
```

We now have a baseline that we can compare our compression methods again. Minification is the first step to make JavaScript small, so let's apply minification. Here are the results:

```text
TypeScript: 410.6kb
WASM:       624.3kb
```

The WASM code is mostly binary code, so the minifier cannot really change it. That's important to keep in mind since typical JavaScript build pipelines won't much effect on WASM code. 

Now let's take a look at gziped sizes for our minified code. Here are the results after minifying and gzipping:

```text
TypeScript: 100.7kb
WASM:       226.6kb
```

The minified TypeScript code compresses a lot easier than the WASM code does. We've compressed our TypeScript code by over a factor of 8 while our WASM code only got compressed by a factor of maybe 3. That's a huge difference.

This highlights an issue that's bugged me with a lot of WASM frontend code - it's rather difficult to make data transfer fast. WASM code is at the mercy of it's compiler toolchain for how big the final binary is, and it's really hard to get that final binary down in size. 

## Ending Thoughts

The main the promises of WASM I've hard is better performance and code reuse (especially with servers not written in Node). Those are both great ideas, but I haven't seen them really pan out.

Performance can be better, but it's not a guarantee. Going from WASM land to JavaScript land is slow. The more times it's needed, the smaller the performance gain will be. Also, the lack of compressibility could start causing loading issues slower than equivalent JavaScript code.

Reusing code is also a mixed bag. The memory model brings manual memory management to automatic memory management languages. This can compicate the code and it makes JavaScript feel a lot more like a poorman's C than a language for the web. It also requires that you have a server language that can be turned to WASM (and if you want speed then it better be a systems langauge).

I also haven't ever reused server code in the browser - even when using Node. The browser and server just have very different concerns. Plus HTMX makes it so that we don't need to run much (if any) code on the browser. So I really don't see the need to move native code to the browser.

The one place I could see use for WASM is wrapping native code without having to learn the addon/FFI model for all of the JavaScript runtimes now (e.g. Node, Deno, Bun, Vercel Edge Runtime, etc). It's also good for the C/C++ devs who just want to use existing code in the browser and don't mind manual memory management. Other than that, I can see why it hasn't taken off and why everything seems to just get rewritten in JavaScript, even when there's an open source C library that does the same thing.

