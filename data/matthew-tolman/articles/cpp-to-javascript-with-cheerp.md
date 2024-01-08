
Lately I've been exploring different ways to make C and C++ code run in the web browser. I compiled one of my [C/C++ libraries](https://matthewtolman.dev/wasm/random/) to WASM with Emscripten and Embind (details in my post [[article:creating-a-wasm-library-with-embind]]). I also made [a maze program in C and SDL2](https://matthewtolman.dev/wasm/interactive-maze/) and got that working via Emscripten.

Emscripten is great, but it only compiles to WASM. WASM is an instruction set, which means that it compiles to processor operations (in this case a virtual processor). WASM programs must include their own memory management, standard library, and any communication layers for interactions with the outside world (e.g. JavaScript, DOM, Web APIs). The amount of code needed to ship can cause pretty large file sizes that rival or even exceed "modern web apps" with heavyweight frameworks and dependencies.

> Technically Emscripten can compile to asm.js which is the JavaScript-subset precursor to WASM. However, asm.js is extremely restricted to a very crude "instruction set", so it's not much better than WASM (and is probably worse since WASM is more feature rich).

As I've been experimenting with WASM, I had an interesting question - what if we could bypass WASM and compile directly to JavaScript? If we could do that, then we could use the libraries and garbage collector built into the browser instead of having to distribute our own versions. We also wouldn't need a translation layer to interact with the DOM since we are creating JavaScript that can access the DOM directly.

As I've been looking at tooling, I discovered that [Cheerp](https://leaningtech.com/cheerp/) can compile C++ to JavaScript. So I decided to give it a try.

[toc]

## What is Cheerp?

Cheerp is a compiler toolchain for C++ which can compile C++ code to JavaScript. This means it comes with a compiler, linker, standard library, and some common build system integrations (e.g. CMake) to take C++ code and compile it into WASM (and yes, compiler toolchain for C++ really are that complicated).

> Cheerp can also handle compiling C++ code to WASM (including compiling only portions of code to WASM), though I haven't tried out that feature so I won't be discussing it. To me, the interest for this post was the JavaScript compilation.

To get a basic idea of how Cheerp feels, here is some sample C++ code.

```cpp
#include <cheerp/client.h>
#include <cheerp/clientlib.h>

// Without this, you write client::document instead
//   and client::console
using namespace client;

int main() {
  document.querySelector("#id")
	->get_classList()
	->add("my-class");

  document.querySelector("p:first-of-type")
	->set_textContent("Hola");

  console.log("Hello from C++!");
  return 0;
}
```

For C++ code, it's surprisingly readable. Some JavaScript developers may also recognize the JavaScript DOM APIs being used, as well as the console printing. That is because Cheerp exposes the browser API to C++ for direct use, which ends up making the code feel more like C++-ified JavaScript rather than normal C++ or JavaScript code.

## Interacting with The DOM

When it comes to interacting with the DOM, Cheerp is pretty straightforward. I was able to rely mostly on MDN's JavaScript documentation, my experience as a web developer, and auto-complete to quickly create a [Todo application in C++](https://matthewtolman.dev/js/cheerp-todo/). There were very few gotchas. The only one I found was that some attributes required a static cast to a specific element type (such as the `value` field on inputs). Other than that, things worked as expected, which was really nice.

Here's my C++ code (minus a large string with CSS styles in it):

```cpp
#include <cheerp/client.h>
#include <cheerp/clientlib.h>
#include <assert.h>

constexpr double ENTER_KEY = 13;

int main() {
    auto nextId = std::make_shared<uint64_t >(0);

    auto* styleTag = client::document.createElement("style");
    styleTag->set_innerText(R"css(
 # Omitted for brevity, most apps would put this in a separate file anyways
)css");
    client::document.get_head()->appendChild(styleTag);

    auto* app = client::document.querySelector("#app");
    assert(app);

    auto* header = client::document.createElement("h1");
    assert(header);
    header->set_innerText("Todo List");
    app->appendChild(header);

    auto* input = static_cast<client::HTMLInputElement*>(
        client::document.createElement("input")
    );
    assert(input);
    input->setAttribute("type", "text");
    input->set_id("new-item");

    auto* label = client::document.createElement("label");
    assert(label);
    label->setAttribute("for", "new-item");
    label->set_innerText("New Item");

    auto button = client::document.createElement("button");
    assert(button);
    button->set_innerText("+");

    auto* inputDiv = client::document.createElement("div");
    assert(inputDiv);
    inputDiv->appendChild(label);
    inputDiv->appendChild(input);
    inputDiv->appendChild(button);
    inputDiv->get_classList()->add("new");
    app->appendChild(inputDiv);

    auto todos = client::document.createElement("div");
    todos->get_classList()->add("items");
    app->appendChild(todos);

    auto addTodo = [input, todos, nextId]() -> void {
        auto value = input->get_value();
        if (value->get_length() == 0) {
            return;
        }

        auto id = "td-item-" + std::to_string(*nextId);
        *nextId += 1;

        auto todoItem = client::document.createElement("div");
        todoItem->get_classList()->add("item");

        auto todoCompleted = static_cast<client::HTMLInputElement*>(
            client::document.createElement("input")
        );
        todoCompleted->set_type("checkbox");
        todoCompleted->set_onchange(cheerp::Callback([todoItem](){
            todoItem->get_classList()->toggle("done");
        }));
        todoCompleted->set_id(id.c_str());
        todoItem->appendChild(todoCompleted);

        auto todoText = client::document.createElement("label");
        todoText->set_innerText(value);
        todoText->setAttribute("for", id.c_str());
        todoItem->appendChild(todoText);

        auto todoDelete = static_cast<client::HTMLButtonElement*>(
            client::document.createElement("button")
        );
        todoDelete->set_innerText("-");
        todoDelete->set_onclick(cheerp::Callback([todoItem, todos](){
            todos->removeChild(todoItem);
        }));
        todoItem->appendChild(todoDelete);

        todos->appendChild(todoItem);
        input->set_value("");
    };
    button->set_onclick(cheerp::Callback(addTodo));

    auto keypress = [addTodo](client::KeyboardEvent* event) {
        if (event->get_keyCode() == ENTER_KEY) {
            event->preventDefault();
            addTodo();
        }
    };

    input->set_onkeypress(cheerp::Callback(keypress));

    return 0;
}
```

`cloc` counts the above snippet as less than 77 lines of code, and that's for a functional todo list app in C++. It's the least code intensive C++ app that I've ever written. It's pretty impressive how capable and concise C++ feels when using Cheerp.

I don't mind using Vanilla JS APIs to get things done, so I wasn't bothered by it. However, the biggest gripe I can see from most web developers is that there isn't a framework like React or Angular for Cheerp. In my mind, it's not an issue and for most C and C++ developers, verbosity of setting each attribute as a separate command is both not an issue and actually surprisingly concise compared to most APIs they work with (have you tried rendering a teapot with OpenGL? and that's "the easy but slow graphics framework"). This is to say that Cheerp may appeal to C++ developers, but it probably won't appeal to JavaScript developers. I'll revisit this point later, but for now let's move on.

## Creating Libraries

Cheerp can export functions and code, which is pretty cool, but it has some warts. The first wart is that Cheerp's export methods are intrusive, while Emscriptens are non-intrusive. For example, here's how we'd export an existing method in Emscripten:

```cpp
// header.hpp
#pragma once

int some_func(int a, int b);


// bindings.cpp
#include <emscripten/bind.h>
#include "header.hpp"

EMSCRIPTEN_BINDINGS(MyModule) {
    function("someFunc", some_func);
}
```

With emscripten, we didn't need to change the original header file. Instead, we declare "hey, I'm going to export this" and then emscripten exports it. It's really nice. It makes the code way less cluttered. It's even nicer when working with a library used on the web, desktops, embedded devices, mobile, etc since there is one less macro or annotation cluttering the header files.

Unfortunately, Cheerp doesn't do this. Instead, it requires adding compiler annotations to code, like so:

```cpp
// header.hpp

[[cheerp::jsexport]]
int some_func(int a, int b);
```

It is less code overall (by a lot), but it does require going in and changing existing code to make a function available. That may or may not work for some projects.

I also had issues where the docs say that developers must call the `delete` method on a C++ object (similar to Emscripten), but not all objects got a delete method. From what I can tell, it's only needed if there is a non-trivial destructor (e.g. manual memory deallocation) going on. If all the memory for an object ends up  in "JavaScript land" (e.g. using JavaScript strings instead of C++ strings), then a `delete` method doesn't get added. I didn't play around with it too much, but the inconsistency was a bit annoying. I'd much rather have a delete method always appear so I can write code like the following:

```javascript
const cppValue = someCppFunc()
try {
    // do stuff
}
finally {
    cppValue.delete()
}
```

Rather than having to write the following code:

```javascript
const cppValue = someCppFunc()
try {
    // do stuff
}
finally {
    if (Object.hasOwnProperty(cppValue, 'delete')
        && typeof cppValue.delete === 'function') {
        cppValue.delete()
    }
}
```

But we're not done with issues exporting objects. Cheerp also doesn't allow exporting properties. It only allows exporting getters and setters. So instead of doing something like the following:

```cpp
[[cheerp::jsexport]]
struct MyStruct {
    int a;
    int b;
};
```

we have to write the following:

```cpp
[[cheerp::jsexport]]
struct MyStruct {
    int a;
    int b;
    int c;
    
    // Yes, we must export every method as well
    [[cheerp::jsexport]]
    int getA() const { return a; }
    
    [[cheerp::jsexport]]
    void setA(int newA) { a = newA; }
    
    [[cheerp::jsexport]]
    int getB() const { return b; }
    
    [[cheerp::jsexport]]
    void setB(int newB) { b = newB; }
    
    // Nothing for member `c`, so it's not exported
};
```

That is a lot more code to export a class. It's also way more intrusive, and it means we cannot export C structs directly since C doesn't have getters and setters. Instead, we need to create a C++ wrapper for any C struct we want to export, and then add getters and setters for every property.

I do understand that sometimes developers don't want to export everything. And sometimes developers don't want an "export by default" option. But a lot of the time, we do want to export all public methods and properties.

At the very least, Cheerp should allow property exports. Also, if Cheerp really wants to lean into compiler attributes, they should consider at least adding two new compiler attributes: `[[cheerp:export_public]]` and `[[cheerp::export_ignore]]`. That would allow developers to export all public members by default, and then optionally ignore the ones they don't want to export. That way our above code becomes this:

```cpp

[[cheerp::jsexport]]
[[cheerp::export_public]]
struct MyStruct {
    int a;
    int b;
    [[cheerp::export_ignore]]
    int c;
};
```

Way simpler, and way more in line with what I would expect. Overall, using Cheerp to build a full application is a good experience. Using Cheerp to export a library needs some work.

One last side note which really bugged me: Cheerp forces async loading. This sucks. My JavaScript code must always wait for a promise to use my library, even on a Node server, even with JavaScript-only output. This causes everything to become async, which is really unpleasant to do. In contrast, Emscripten allows inlining WASM into a JS file and doing a synchronous load, which makes using a library way nicer since it can be used just like any other library. I really hope that Cheerp adds a compilation flag for allowing synchronous loading.

## Output Size

Output size is pretty impressive compared to many modern JavaScript frameworks. Without my bundled CSS, the JS output was 24kB and with the CSS it was around 32kB. That's actually not bad.

> Side note: the debug build was about double the size (probably related to the asserts). For my size comparisons, I'm using my release build with `-O3`.

For comparison, I took a look at several other todo apps written in several JavaScript frameworks to get the comparison sizes. I excluded the CSS sizes to just focus on the JavaScript output. Here's what I found:

| Cheerp | [MDN's Todo (React)](https://github.com/mdn/todo-react)| [MDN's Todo (Vue)](https://github.com/mdn/todo-vue) | [Ferdinando's Todo (Angular)](https://github.com/FerdinandoGeografo/todo-app-fg) |
|--------|--------------------|------------------|------|
| 24kB   | 148kB              | 85kB             | 197kB (+33kB for polyfills)     |

> I chose Ferdinando's todo since that was the only one I could find for Angular which was on the latest version. I chose MDN for the others since I was a little familiar with MDN's tutorials.

Do note that the above does not mean that Cheerp is always smaller. I'm sure someone can come up with a Cheerp todo app that's much bigger. It just indicates that we're within (and probably below) the output size we'd expect from other frameworks.

However, the issue with the comparison is that Cheerp is not a framework. It's a C++ flavor of JavaScript. Since it's just a language translation, it's more fair to see the cost of that translation. So, I rewrote my Todo app in JavaScript and took the measurements.

Here are the results:

| Measurement | Cheerp | Vanilla JS |
|-------------|--------|------------|
| LoC         | 77     | 66         |
| Output size | 24kB   | 1.3 kB     |

The output sizes are minified for both. We can see that we have fewer lines of code and a smaller output when using plain JavaScript.  The cost of Cheerp lies somewhere between doing it all in plain JavaScript, and using a JavaScript framework. For C++ developers, this is a win since the end application can still be smaller than framework based applications. For JavaScript developers, it's not as useful since they'd still be using DOM APIs, but they'd loose some of the main benefits of plain JavaScript (e.g. no build system or toolchain, small output, etc).  

## Quirks

So far we've been talking about what Cheerp can do. But Cheerp's JavaScript compilation has quirks which add limitations that aren't present in any "compile to web assembly" toolchain. Some C and C++ developers will probably get annoyed when using Cheerp's JavaScript model on pre-existing code code. Unfortunately, there isn't too much that can be done other than treat Cheerp as a separate, not fully standards compliant platform.

So for a second, let's talk about how compilers work for C and C++ at a high level, and then we'll take a look at Cheerp to see why there are quirks.

### Traditional Compilers

Traditional compilers for C and C++ target CPU instruction sets. This means they generate assembly instructions which can load memory from any address, write memory to any address, switch registers between integer and floating point operations, use a stack, have RAII access, etc.

> There are some restrictions enforced from the operating system about memory access (e.g. segfaults if accessing memory outside of the program memory). However, C and C++ programs usually have free reign to access any memory reserved for the program, and that's a pain point when converting to JavaScript code.

This means that if someone writes the following code, it will compile just fine:

```c
void* alloc() {
  return malloc(96);
}

float do_something() {
  int* i = alloc();
  *((double*)(void*)i) = 0.0;
  ((char*)(void*)i)[1] = 'a';
  float res = *((float*)(void*)i);
  free(i);
  return res;
}
```

In the above code, we're allocating memory, access and write different chunks with different types, copy the memory to a register, and free the memory. It's definitely not good C/C++. But it shows what sort of shenanigans C/C++ code (especially older code) can get up to. Most compilers handle the above code just fine, including WASM compilers. However, Cheerp's JavaScript compiler struggles a lot.

### The Cheerp Compiler

Unlike most other compilers, Cheerp isn't compiling to an instruction set where it has access to arbitrary memory. Instead, it's compiling to JavaScript. This means that for every operation and feature of C and C++, Cheerp has to use an equivalent JavaScript feature. If Cheerp cannot find an equivalent feature, then the operation cannot be translated and the code cannot be compiled. When this comes to memory, Cheerp can only translate operations that work with discrete values. It cannot work with code that treats memory as a big pile of bytes that can be casted and manipulated freely.

For us, this means we cannot compile the above code using Cheerp's JavaScript compiler. The first issue is that we're using malloc and returning a `void*`. Cheerp doesn't know what to do here, so it throws a compiler error. The direction we receive is to do a type cast from malloc to a concrete pointer type (e.g. `int* a = (int*)malloc(sizeof(int));`). This means that any code which allocates memory but doesn't cast it (e.g. a malloc wrapper that does memory leak detection) can't be compiled to JavaScript with Cheerp.

So, let's fix our code and inline our allocation. We end up with the following:

```c
float do_something() {
  int* i = malloc(96);
  *((double*)(void*)i) = 0.0;
  ((char*)(void*)i)[1] = 'a';
  float res = *((float*)(void*)i);
  free(i);
  return res;
}
```

We no longer get compilation errors, but we get a lot of compiler warnings of *undefined behavior*. Cheerp can't just take a bunch of bytes and start casting them to different pointer types like we're doing above. We start with an array of ints, turn them into an array of doubles, then an array of characters, then an array of floats, and then we deallocate them. And to be clear, we aren't *converting* the array to different values. We aren't saying "hey, lets properly convert the ints into doubles." We're just taking their bytes and then treating them as if they were doubles all along (even though they probably aren't). JavaScript isn't setup to do that type of operation. It does conversions, but not binary reinterpretation. Cheerp does its best, but the behavior is not guaranteed, so these operations become undefined behavior when compiling to JavaScript.

That said, when I've written new code with Cheerp in-mind, things have just worked. Most of the standard library features I've tried just work, including newer things like `std::optional`. Printing to `stdout` also worked. Lambdas worked. It was actually a really nice experience when writing new code. Old code may just need to be compiled to WASM.

## Community

The Cheerp community exists almost exclusively on their Discord server. The server is somewhat active and the people are pretty friendly. I've seen help for issues that aren't even related to Cheerp itself but is more of a C++ standards questions.

But, I've struggled finding anything about Cheerp on search engines (I've tried several) other than on official websites or blog posts made by maintainers of Cheerp. I also could ony find one or two answers on stack overflow (out of dozens of different searches). However, the answers I did find were for questions that could have been answered by the official docs. Nothing more complicated could be found (e.g. Cheerp-specific compile messages).

If you really like getting answers and help through direct communication, then Cheerp will be good for you. If you really just want to run a search to get an answer, then you'll probably be disappointed with Cheerp (at least for the short term).

## Conclusion

Cheerp is an interesting toolchain. It takes C++ and says, "you'll become JavaScript like everything else." The result is something that's really cool. A lot of C++ code can be compiled just fine. And the output is way smaller than what you'd get using just WASM. Plus there's quite a bit of advertised features that I haven't explored in this post (WASM compilation, partial WASM and partial JavaScript output, etc). The team behind Cheerp are also working on a lot of other tools, like Java to JavaScript, Flash to JavaScript (for old Flash games), and WebVM which can run x86 executables in the browser. There's some cool stuff going on.

That said, I don't know who Cheerp's C++ to JavaScript compilation is targeting. Most frontend developers I know don't want to use C++. Most C++ developers I know don't want to learn DOM APIs and would be better served with HTMX. Most C++ applications I've seen people try to port to the browser end up getting better served by Emscripten since so many hours have gone into fixing a bunch of gaps (and it's way better documented for that).

I still find Cheerp fascinating. It's taken an incredibly tough (and weird) goal of turning C++ to JavaScript, and it's done way better than I could have hoped for. But it also doesn't provide a lot of benefit over just writing code in JavaScript. It requires knowing the JavaScript APIs to be usable, and it's not able to port any C++ code over without leaning heavily on WASM. Sure, it does allow developers to use some C++ libraries they couldn't before, and it allows using C++ on both the server and the backend. However, I haven't really found either of those to be needed or beneficial.

That's not to say I don't like Cheerp. I found that I actually really liked it. And I can see the mindset which led to the project's development. But I can't help but feel like it's a cool technology that is in search for a problem. Of course, I've been feeling that a lot with plain WASM toolchains too, so the fact that Cheerp has a JavaScript and WASM target makes it more interesting than Emscripten.
