One of the most frustrating yet fascinating facets of using a programming language is how it handles errors. Many of
these error handling patterns include the traditional try/catch, error flags, unions, multiple return values, error
paths, etc. These patterns are fascinating to investigate and see the various methodologies and practices
languages and libraries have used to implement them. This is by no means an exhaustive list or investigation. Rather,
it's a review of what I've personally experienced with error handling.

> I prefer the term "error" rather than "exception" since there's nothing exceptional about exceptions. The term
> "exception" makes it seem like this is some obscure edge case, but in reality they are extremely common (e.g. service
> unavailable, host not found, page not found, permission denied, file does not exist, dereferenced null pointer, invalid
> email, missing required field, etc.).
> 
> The term "error" doesn't hold the same connotation that "exception" does, which is why I prefer the terms "error" and
> "error handling" and I'll be using them throughout this article. If you prefer the terms "exception" and "exception
> handling," fell free to mentally substitute those terms whenever you see "error" and "error handling."

[toc]

## The try/catch model

The try/catch model is one of the most prevelant error models out there. In essence, code will "throw" an error up
the call stack until somebody "catches" it. Once someone catches the error, they can try to resolve it or throw one of
their own errors. If nobody catches the error, then the program catches. Below is some sample code:

```javascript
function thrower() {
    throw new Exception("Something happened")
}

function catcher() {
    try {
        thrower()
    }
    catch (e) {
        console.log("Caught exception!")
    }
}

// Doesn't crash the program
catcher()

// Does crash the program
thrower()
```

Below is the output from running the program:

```text
Caught exception!
Uncaught ReferenceError: Exception is not defined
    thrower code:2
    <anonymous> code:18
    thrower code:2
```

This model seems pretty straightforward at first. Any code which throws an error should get wrapped in a try/catch.

### Inability to know when to handle errors

The first issue with the try/catch arises when error throwing starts happening deep in the callstack, or when we
call code in a library (including the language's standard library) where it's not obvious when we throw. For instance,
consider the following code:

```javascript
const importedCode = require('some-library').init()

importedCode.runAction();
```

Which lines require us to use a try/catch? Which lines don't? We could try to go through the documentation, but not
every library documents what is thrown. If this was a first-party library (e.g. an internal company library), the chances
that it's properly documented drop quickly.

There have been attempts to resolve this issue. For example, Java requires most uncaught errors to be documented as part
of the method signature. For example, consider the following code: 

```java
public class Main {
    // we catch our exceptions, so don't add it to the signature
    public static void catcher() {
        try {
            thrower();
        }
        catch (Exception e) {
            System.out.println("Caught exception!");
        }
    }

    // We throw, so it has to be part of the signature
    public static void thrower() throws Exception {
        throw new Exception("Something happened");
    }

    // We call a method that throws, but don't handle it ourselvs
    // we must declare that in our signature
    public static void main(String[] args) throws Exception {
        catcher();
        thrower();
    }
}
```

Requiring code to document when it throws seems like the solution. Whenever we declare an error a method must make that
explicit and everything's good, right?

Well, not quite. One common trend I've noticed from these systems is they usually include ways to "hide" errors. For
instance, we can write the following code.

```java
public class Main {
    public static void thrower() {
        throw new RuntimeException("Something happened");
    }

    public static void main(String[] args) {
        // will end the program since the exception is not caught
        thrower();
    }
}
```

One of the reasons for this bypass system to exist is so that develoeprs can extend "non-throwing" interfaces with code
which might throw. For instance, consider the following code:

```java
interface NumberGenerator {
    // Since we don't have a "throws" as part of the signature,
    //  no implementation may have a "throws" as part of their
    //  method signature either
    int number();
}

class StaticNumberGenerator implements NumberGenerator {
    // No exceptions, no problem
    @Override
    public int number() { return 4; }
}

class RequestNumberGenerator implements NumberGenerator {
    // We can throw, but we can't have those exceptions
    //  as part of our signature since our interface
    //  doesn't have it as part of it's signature
    @Override
    public int number()  {
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create("https://example.com/"))
                .timeout(Duration.ofMinutes(2))
                .header("Content-Type", "application/json")
                .GET()
                .build();

        var client = HttpClient.newBuilder()
                .version(HttpClient.Version.HTTP_1_1)
                .followRedirects(HttpClient.Redirect.NORMAL)
                .connectTimeout(Duration.ofSeconds(20))
                .authenticator(Authenticator.getDefault())
                .build();
        try {
            // this line can throw different exceptions,
            // but since we're imp
            HttpResponse<String> response = client.send(
                    request,
                    HttpResponse.BodyHandlers.ofString()
            );
            return response.body().length();
        }
        catch (Exception e) {
            // The "workaround" is to convert all of our exceptions to
            //   a RuntimeException so that we can throw
            throw new RuntimeException("Could not fetch resource", e);
        }
    }
}
```

In the above code, we have an interface which doesn't let its member functions throw errors. This is pretty common,
especially when the interface is defined in one library but the implementation is defined in another (e.g. a library
allows developers to provide a custom implementation of an interface).

However, since Java chose to have the errors as part of the method signature, we can't "throw" an error in the
implementation. If we did, that would be a violation of the interface specification! But we may not always be able to
correct the error in our implementation. For instance, what if in some cases we wanted to try the `RequestNumberGenerator`
implementation first, but then fallback to the `StaticNumberGenerator` implementation? If the `RequestNumberGenerator`
wasn't allowed to tell us if it failed, we wouldn't be able to implement our fallback logic.

The workaround is to "hide" the errors that are thrown by using `RuntimeException`. This allows us to throw whenever we
need to without changing the interface. But, now we're back to square one. We weren't able to identify when we threw
an error message which meant we didn't know when we needed a try catch, so we just required everything to annotate when
it throws an error, but then we ran into an issue with interfaces, so we decided to allow code to hide when it throws
errors, which means we don't actually know when code throws an error.

In fact, we're in a worse place because now we have a system which lets people *think* they know when an error will be
thrown, but that system is *incorrect* and so now we can have code which *appears* to not throw but in reality it *does*
throw.

Other systems with annotating thrown errors run into similar issues. For instance, PHP lets users annotate the errors
which are thrown. However, interfaces run into the same issue as Java interfaces, so we won't always know when a method
truly throws or doesn't throw.

The inability to reliably know when to add a try/catch is the single largest downfall of the model. It makes it
impossible for developers to reliably know if they've handled errors properly, and all the attempts to remedy the
issue have resulted in partial solutions which don't reliably work.

### Errors and allocated resources

The next biggest pitfall with the try/catch model is around resource allocation. The most commonly allocated resource
developers talk about is memory. However, memory allocation is not the only resource that programs need to manage. It
is important to note that "memory safety" does not guarantee "program safety". A program may be "memory safe", but may
leak other critical resources of a computer or may exhibit other unsafe behavior. Other resources that programs need to
manage include (but are not limited to):

* Network sockets
* File handles
* Database connections
* GPU resources (e.g. logical devices, GPU memory, pipelines, etc)
* Results from FFI calls (or native code wrappers or WASM)
* Globally cached data
* [Observable subscriptions](https://rxjs.dev/guide/subscription)

When an error occurs, failure to properly deallocate any allocated resource can lead to memory leaks, further errors,
and program misbehavior. There have been a few approaches to allow for deallocating resources in the face of an error.
We'll walk through these and discuss them.

#### finally

The "finally" block is one of the most common patterns for resource deallocation. When a try block is declared, a
"finally" block can be added to the end of the try block. The "finally" block is guaranteed to run before the scope
is left. This allows developers to add resource deallocation code. Below is an example:

```java
class ExampleFile {
    public void write(String contents) throws IOException {
        var file = new FileWriter("example.txt");
        try {
            file.write("hello world!");
        }
        finally {
            file.close();
        }
    }
}
```

### using pattern

The using pattern is another common pattern. Resources can be declared as "disposable", and used resources are declared
inside a "using" statement. The using statement will then guarantee that the allocated resources are available inside
a block of code, and it will properly deallocate or dispose of those resources once the program execution has left that
block of code.

> The term "using" comes from C\#'s "using" keyword. However, there are many different keywords that this pattern appears
> with. I've seen "using", "use", and "try". 

Below is an example:

```csharp
using (StreamReader exampleFile = File.OpenText("file.txt"))
{
    string line;
    while ((line = reader.ReadLine()) is not null)
    {
        Console.Write(line);
    }
}
```

### RAII

The RAII (Resource Acquisition Is Initialization) pattern is mostly specific to C++. In this pattern, the lifetime of
an allocated resource (memory, file handle, socket, etc.) is bound to the lifetime of an object. Once that object's
lifetime ends, the corresponding resource is automatically deallocated. This allows for powerful constructs which allow
for automatic resource management. For instance, below is code which does both automatic memory management and automatic
file handle management:

```cpp
#include <memory>
#include <filesystem>
#include <cstdio>
#include <vector>
#include <optional>
#include <iostream>

class ReadFileHandle {
    FILE* cFileHandle;
public:
    ReadFileHandle(const std::filesystem::path& filePath)
        : cFileHandle(std::fopen(filePath.c_str(), "r")) {}

    ~ReadFileHandle() {
        if (is_valid()) {
            // Close our file handle automatically
            std::fclose(cFileHandle);
        }
    }

    [[nodiscard]] auto is_valid() const -> bool { return cFileHandle != nullptr; }

    [[nodiscard]] auto read_file() const -> std::optional<std::string> {
        if (!is_valid()) {
            return std::nullopt;
        }

        // Read the entire file into memory
        std::fseek(cFileHandle, 0, SEEK_END);
        size_t size = ftell(cFileHandle);
        auto buffer = std::string(size, '\0');
        std::rewind(cFileHandle);
        std::fread(buffer.data(), sizeof(char), size, cFileHandle);
        return buffer;
    }
};

int main() {
    // Allocate memory
    auto fileHandle = std::make_unique<ReadFileHandle>(
        std::filesystem::current_path() / "example.txt"
    );
    
    // Print the contents of the file (or an error message)
    std::cout << fileHandle->read_file().value_or("<could not open file>") << "\n";

    // No need to manually deallocate memory, that's done automatically
    // Also, no need to close the file handle, that's also done automatically
    throw std::runtime_error("some error happened");
}
```

### defer and scope exit

The defer pattern defers some block of code to run once the scope is left. This is useful for both deallocating
resources normally, and deallocating resources during an error. Usually, defer is at scope exit, though some languages
(e.g. Go) are at function exit.

For instance, here's a Zig program using scoped defer:

```zig
const std = @import("std");

pub fn main() void {
	{
		defer std.debug.print("Print 1\n", .{});
	}

	std.debug.print("Print 2\n", .{});
	
	{
		defer std.debug.print("Print 3\n", .{});
	}
}

// Output:
//  Print 1
//  Print 2
//  Print 3
```

Here's a Go program using function defer:

```go
package main

import ("fmt")

func main() {
	{
		defer fmt.Println("Print 1")
	}

	fmt.Println("Print 2")

	{
		defer fmt.Println("Print 3")
	}
}

// Output:
//  Print 2
//  Print 3
//  Print 1
```

The difference in scope defer vs function defer can lead to some interesting gotchas when transitioning between the two
systems. However, for the purposes of resource deallocation, either methodology works, so we'll treat them as equally
capable for the rest of the article.

The defer statement will usually appear immediately after a resource is allocated. This will guarantee that the resource
is properly deallocated, even when an error occurs. Below is an example:

```go
func DoSomethingWithFile(fileName string) {
    src, err := os.Open(fileName)
    if err != nil {
        return
    }
    // Guarantees we close our file handle
    defer src.Close()
    
    // Do what we need to with the file here
}
```

Languages with built-in defer (or equivalent) support include:
* Zig^[zig-defer]
  * Uses scoped defer
* Go^[go-defer]
  * Uses function defer
* D^[d-scope-exit]
  * Uses scoped guards which is similar to scoped defer

Additionally, C++ can mimic scoped defer by combining RAII, lambdas, and C-macros^[cpp-defer].

### Control Flow

The try/catch pattern does introduce more complicated control flow. Any statement could theoretically result in an error,
and that error will change the flow of the program. Extra care needs to be taken to ensure resources are properly
deallocated. Additionally, debugging and understanding code can get more complicated since the control flow is not
linear (i.e. it does not strictly follow how the code is read since it may jump higher in the call stack at any point).

## panic/recover model

The panic/recover model is similar to the try/catch model. The main difference is that try/catch tends to be general for
all error types (including user-facing errors and recoverable errors) while the panic/recover model is designed for
"unrecoverable" errors^[go-panic]. In other words, even though panic/recover has surface similarities to try/catch, it
is designed and designated as a "last resort" error model instead of a "common use case" error model. In these systems,
another error handling model is designated as the "primary" error handling mechanism.

Do note that the presence of "recover" is important as it allows for partial system failures and then further recovery^[go-panic].
For instance, if there is a pool of worker threads and one of them panics, the thread pool could recover the panic and
prevent a whole system crash. This is especially important with web servers where recovery would prevent panicked
requests from crashing the whole server. Additionally, recover could be used to respond with a standard 500 and a graceful
TCP socket connection, rather than an abrupt connection close without a response.

## Errors as values

The "errors as values" model uses return values to indicate if there was an error, and if so what the error was. There
are several different "flavors" of the errors as values model. We'll focus on the most significant flavors: error flag
and global error, error flag and out param, error flag or result, multiple return with error and result, and tagged unions.

The general advantage of the errors as values pattern is that code which uses "errors as values" will execute in the
same read order. There aren't hidden jumps up the call stack. This "same read order" lowers the need for complicated
resource deallocation schemas (though they are often still useful to have when there are multiple return statements).

Additionally, developers don't have to aggressively add in try/catch statements to "catch all possible errors." Instead,
developers can look at the function return values and know what error(s) they may have to deal with.

One disadvantage is error propagation. If a piece of code cannot handle an error, it will need to pass the error up to
the caller. Doing so will require a change in the return type parameter.

Personally, I really like the errors by value approach. It avoids quite a few issues with the try/catch model, and it
brings a lot of niceties. However, not all flavors of the pattern are equal. Most of the patterns I'll cover are not
good patterns and should be avoided.

### Error flag and global error

This model is most common in C code, though it will also appear in other languages (I've seen it in PHP). The basic idea
is that code will return whether it passed, and it will set a global variable with details about the error.

Generally, this code is meant to only run in a single threaded context (such as during program initialization). Running
code like this in a multithreaded context can cause race conditions since multiple threads could be trying to set the
global error variable at teh same time. Below is an example of this pattern using the SDL^[sdl] library:

```c
int main (int argc, char **argv)
{
  // Initialize SDL. If we get an error code, print the error
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL failed to initialise: %s\n", SDL_GetError());
    return 1;
  }
  
  // code goes here
  
  // Clean up SDL
  SDL_Quit(); 
}
```

This pattern does not hold up in a multithreaded environment, so it shouldn't be used regularly.

Another disadvantage is that this error pattern usually cannot enforce that errors are handled or checked for (though
some languages have added features to require that a return value is used, such as C++'s `[[nodiscard]]`^[cpp-nodiscard]).
This means that an error which should be addressed can go ignored which can cause further issues and bugs in the program.

The reliance on a global variable for setting error information can cause other issues when developers either don't
understand the pattern or they don't fully adhere to the pattern. Sometimes developers will ignore the return flag
and check the global variable, or they won't return a flag and only set the global on an error. In these cases, it's
quite possible to get incorrect error detection logic - especially if an error was ignored earlier in the code.
Below is some PHP code which shows this behavior (this is based on real code in a real production environment):

```php
<?php
$lastError = null;

function willFail() {
  global $lastError;
  // Set our error message
  $lastError = 'Called willFail';
  return false;
}

function willPass() {
  return true; // indicate success
}

willFail();

// We should check for an error, but we don't

// ... lots of other code here

willPass();

// Yes, some devs would check the global
//  and not the returned flag
if (!empty($lastError)) { 
  echo "willPass failed! Error: {$lastError} \n";
}
else {
  echo "willPass succeeded!\n";
}
```

This pattern has outlived its usefulness. We are now in a multithreaded age, so having a single-threaded pattern
heavily limits what we can do. Additionally, I've had to work with enough code where developers would ignore critical
errors, which then caused code which was trying to handle errors to handle an error incorrectly (similar to the above
example). A lot of old code still uses this pattern, so it's good to know what the pattern is and how to work with it
when needed. However, it shouldn't be used in new code.

### Error flag and out param

This is very similar to the above pattern, and sometimes it even has overlap. The general pattern is that a function
will accept a pointer or reference to an "output variable". If the funciton succeeds, it will return a success flag and
set the output variable. If the function fails, it will return a failure flag.

When error details are needed, the function will either set a global variable (similar to the previous pattern), or it
will take a pointer/reference to an error output variable. Below is an example program in C99:

```c
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    const char* errMsg;
} Error;

bool try_random(int* out, Error* err) {
    int val = rand();
    if (val % 5 == 0) {
        err->errMsg = "Number divisible by 5";
        return false;
    }
    *out = val;
    return true;
}

int main(int argc, char** argv) {
    int val;
    Error err;

    srand(time(NULL));
    if (!try_random(&val, &err)) {
        printf("Error occurred: %s\n", err.errMsg);
    }
    else {
        printf("Value: %d\n", val);
    }
}
```

This pattern is mostly outdated and should be avoided when a language provides other alternatives. If it is needed, do
at least accept an output parameter for the error instead of setting a global variable, that way the code can be thread
safe.

### Error flag or result

This pattern I've mostly seen in dynamically typed languages, such as PHP. The basic idea is that a method will return
`false` (or some other invalid value) if and only if there was an error. Otherwise, the success result will be returned.
This pattern is extremely prevalent in PHP code, especially since the PHP standard library uses this pattern (e.g.
fopen^[php-fopen]).

```php
<?php

function readFile(string $filename) : ?string {
  $fileHandle = fopen($filename, 'r');
  
  if ($file === false) {
    // there was an error opening the file
    return null;
  }
  
  try {
    $contents = fread($f, filesize($filename));
  }
  finally {
    fclose($f);
  }
  
  return $contents;
}
```

This pattern can work sometimes when there is a value that definitively won't be a success value. For instance, when
getting a file handle, the value `false` is not going to be a possible success value. Similarly, when searching for the
index of something, a negative value is generally not a valid value (e.g. JavaScript's indexOf^[js-indexof]).

```javascript
function hasElem(arr, elem) {
    return arr.indexOf(elem) > 0;
}
```

However, there are a few downsides to this approach. The first is that it's very easy for someone not familiar with the
language or a particular method to accidentally interpret the error value as a success value. For instance, someone
could write code like the following:

```javascript
// Drop all the elements up to the first occurrence of elem
// (also, make sure we keep the first occurrence of elem)
function dropUpTo(arr, elem) {
    return arr.slice(arr.indexOf(elem))
}

console.log(dropUpTo([10, 12, 23, 34], 12)) // [12, 23, 34]
console.log(dropUpTo([10, 12, 23, 34], 23)) // [23, 34]
console.log(dropUpTo([10, 12, 23, 34], 19)) // [34]
```

This type of code doesn't "crash" the program, but it doesn't behave as intended either. If something isn't found, we
probably drop everything or throw an exception, but the issue is we're keeping the last item. This happens since
`indexOf` will return `-1` if an element isn't found, and `slice` takes negative numbers to mean "keep this many items
from relative to the back of the array."

The other big issue with this pattern is we have to look through the documentation carefully to know what the "error"
indication value is, and we need to do this for every method that we use. If we forget to look up the documentation, or
if we get two methods mixed up, we could easily end up in a situation where we think we got the code right, but in
reality we have a hidden bug.

Statically typed languages do sometimes provide us with tools to make this pattern work. However, for dynamic languages
it can cause a lot of confusion very quickly.

### Multiple return: error and result

With this pattern, each function that could have an error will return two outputs. One of the outputs is the successful
result, the other is the error. Generally, the error value will be set to a "none" value if there was no error (e.g. null).

Go uses the multiple return value pattern^[go-errs-vals].

```go
package main

import (
       "fmt"
       "os"
)

func processFile(filename string) (contents string, err error) {
  file, err := os.ReadFile(filename)
  if (err != nil) {
    return
  }

  contents = string(file)
  return
}

func main() {
     str, _ := processFile("test.go")
     fmt.Println(str)
}
```

Similarly, Odin^[odin-or_return] also uses multiple return values for errors.

```odin
Error :: enum {
  None,
  Invalid_Argument,
  Access_Denied,
  // other errors here
}

do_something :: proc(i: int) -> (int, Error) {
  if (i == 1) {
    return nil, .Invalid_Argument
  }
  return i, .None
}
```

### Tagged unions

Tagged unions (aka. discriminated unions, variants, disjoint union, etc.) are values which may be of one type or another,
and where there is an indicator specifying which type the held value is. This is very different from C unions where there
is no indication as to what the type is in the union. Instead, this is closer to OCaml variants^[ocaml-var],
C++ variants^[cpp-variant], and Rust enums^[rust-enum].

```ocaml
type 's error =
  | Error
  | Success of 's
  
let v = (Success "value")

let _ =
  match v with
  | Error  -> print_string "Something went wrong"
  | (Success (s)) -> print_string s
```

With tagged unions, the compiler will help guarantee we don't confuse the error and non error values. Additionally,
we're able to use pattern matching to handle the error and non-error cases. The pattern matching allows us to explicitly
handle both error cases.

```rust
fn could_error(i: i32) -> Result<i32, ()> {
    match i {
        0 => Err(()),
        _ => Ok(i)
    }
}

fn main() {
    let res = could_error(1);
    match res {
        Ok(val) => println!("Value: {}", val),
        _ => println!("Something went wrong"),
    }
}
```

Personally, this is my preferred way to handle errors as values. It allows for a single, self-contained value which can
be returned directly or processed.

```cpp
#include <variant>
#include <stdexcept>
#include <iostream>
#include <optional>

template<typename Value, typename Err = std::runtime_error>
using Error = std::variant<Value, Err>;

auto try_random() -> Error<int> {
    int val = rand();
    if (val % 5 < 2) {
        return std::runtime_error("invalid number");
    }
    return val;
}
```

It also allows creating monadic libraries to handle transforming success results, recovering from errors, defaulting
values, etc. These libraries can make working with tagged unions much easier.

Some languages come with tagged unions for error types already. This includes Rust's `Result` type^[rust-result]. Other languages are
working on adding tagged unions for error types, such as C++23's `std::expected`^[cpp-expected].

## Error Paths

With error paths we get two different execution paths, one for "success" states and another for "error" states. The
execution of code can swap between the two different paths, allowing a success state to turn into an error and an error
state to recover.

The most widely used instance of this are JavaScript promises. The `then` functions form the "success" path where
successful values can be operated on. The `catch` functions form the error track where error handling can occur. To go
from the "success" track to the "error" track one needs only throw an exception. To go from the "error" track to the
"success" track, one only needs to return a value.

Below is some JavaScript code that demonstrates the success and error paths.

```javascript
(async () => {
  let p = Promise.resolve(
      (Math.random() * 10) | 0)                
      .then(v => {                             
        if (v % 2 === 0) {                     
            throw new Error(v)
        }
        return v
      })
                              .catch(err => {
                                if (+err.message === 4) {
                                  throw new Error('move along')
                                }
                                return 99998
                              })
      .then(v => {
        return v + 1
      })
                              .catch(err => {
                                return -1001
                              })
      .then(v => {
        return v * 2
      })

  const res = await p
  console.log(res)
})()
```

## Summary

There are many different ways to handle errors in programs. Many programming languages have multiple
error handling patterns available. It's important to know which patterns are available in a language,
which ones are used by the libraries and code you're calling, and how to properly handle errors which
occur.

@References
* zig-defer
  | type: web
  | page-title: Zig Language Reference
  | website-title: Zig
  | link: https://ziglang.org/documentation/master/#defer
  | date-accessed: 2022-09-09
* go-defer
  | type: web
  | page-title: Defer, Panic, and Recover
  | website-title: The Go Blog
  | link: https://go.dev/blog/defer-panic-and-recover
  | date-accessed: 2022-09-09
* d-scope-exit
  | type: web
  | page-title: Statements
  | website-title: DLang
  | link: https://dlang.org/spec/statement.html#ScopeGuardStatement
  | date-accessed: 2022-09-09
* cpp-defer
  | type: web
  | page-title: A Defer Statement for C++11
  | website-title: gingerBill
  | link: https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/
  | date-accessed: 2022-09-09
* go-panic
  | type: web
  | page-title: Effective Go
  | website-title: Go
  | link: https://go.dev/doc/effective_go#panic
  | date-accessed: 2022-09-09
* sdl
  | type: web
  | page-title: About SDL
  | website-title: SDL
  | link: https://www.libsdl.org/
  | date-accessed: 2022-09-09
* cpp-nodiscard
  | type: web
  | page-title: C++ attribute: nodiscard
  | website-title: C++ Reference
  | link: https://en.cppreference.com/w/cpp/language/attributes/nodiscard
  | date-accessed: 2022-09-09
* php-fopen
  | type: web
  | page-title: fopen
  | website-title: PHP Manual
  | link: https://www.php.net/manual/en/function.fopen.php
  | date-accessed: 2022-09-09
* js-indexof
  | type: web
  | page-title: Array.prototype.indexOf
  | website-title: mdn web docs
  | link: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/indexOf
  | date-accessed: 2022-09-09
* go-errs-vals
  | type: web
  | page-title: Errors are values
  | website-title: The Go Blog
  | link: https://go.dev/blog/errors-are-values
  | date-accessed: 2022-09-09
* odin-or_return
  | type: web
  | page-title: Overview
  | website-title: Odin
  | link: http://odin-lang.org/docs/overview/#or_return-operator
  | date-accessed: 2022-09-09
* ocaml-var
  | type: web
  | page-title: Algebraic Data Types
  | website-title: OCaml Programming: Correct + Efficient + Beautiful
  | link: https://cs3110.github.io/textbook/chapters/data/algebraic_data_types.html
  | date-accessed: 2022-09-09
* cpp-variant
  | type: web
  | page-title: std::variant
  | website-title: C++ Reference
  | link: https://en.cppreference.com/w/cpp/utility/variant
  | date-accessed: 2022-09-09
* rust-enum
  | type: web
  | page-title: Defining an Enum
  | website-title: The Rust Programming Language
  | link: https://doc.rust-lang.org/book/ch06-01-defining-an-enum.html
  | date-accessed: 2022-09-09
* rust-result
  | type: web
  | page-title: Error Handling
  | website-title: A Gentle Introduction to Rust
  | link: https://stevedonovan.github.io/rust-gentle-intro/6-error-handling.html#:~:text=Error%20handling%20in%20Rust%20can%20be%20clumsy%20if,so%20any%20error%20can%20convert%20into%20a%20Box%3CError%3E.
  | date-accessed: 2022-09-09
* rust-enum
  | type: web
  | page-title: Defining an Enum
  | website-title: The Rust Programming Language
  | link: https://doc.rust-lang.org/book/ch06-01-defining-an-enum.html
  | date-accessed: 2022-09-09
* cpp-expected
  | type: web
  | page-title: std::expected
  | website-title: C++ Reference
  | link: https://en.cppreference.com/w/cpp/utility/expected
  | date-accessed: 2022-09-09