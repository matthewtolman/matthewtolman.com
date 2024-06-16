Recently I started using Clojure again. It's been a while since I've used it. I'm recording my thoughts and impressions as I get back to using it. The experience doesn't feel like it changed much over the past few years, which is both good and bad. This is more of a post of my thoughts rather than having a clear direction.

## Editor Setup

When I first started again, I used IntelliJ with the Cursive plugin. Both are commercial products that require a subscription. However, they are one of the lowest friction development environments to start Clojure.

Cursive provides a really solid structural editing plugin, REPL integration, and project setup. IntelliJ provides a good JVM tool chain with auto-complete, JDK management, etc.
Overall, it's a good experience. Everything just worked. I also quickly regained my muscle memory. The only big difference is that IntelliJ has gotten more bloated and slower over the years. Other than that, everything just worked.

Once I got my bearings, I started setting up NeoVim for Clojure development. I hadn't ever tried Clojure development with a vim editor (previously I used Emacs, but I don't want to try to get that setup again). This was where things got a little more unpleasant.

First, I had to setup nREPL. nREPL is a network protocol for talking to the REPL from the Emacs space. It allows talking to REPLs both locally and remotely (which can be useful when debugging on a server - but it's also pretty dangerous so don't leave it on in prod). Fortunately there is a standalone version of nREPL that can be used outside of Emacs.

Next I setup a structural editor. Lisps wrap code in parenthesis since lisp code is just a list of tokens. This is cool since your editor can be setup to work on tokens instead of characters. This allows you to manipulate lists very quickly (e.g. extend a list to include the next token, eject the first token out of a list, select the contents of a list, change the wrapping parenthesis to brackets, replace a list with a result from the REPL, etc). Structural editing is super powerful since you stop thinking about manipulating characters or lines and start thinking about manipulating entire code blocks. To illustrate, here's the (theoretical) JavaScript equivalent of what you can do with structural editing:

```javascript
// With structural editing, we can do a single shortcut to change this...
if (sendEmailNotification()) {
  await sendEmail()
  await sendSms()
}

// ... into this
if (sendEmailNotification()) {
  await sendEmail()
} else {
  await sendSms()
}

// Or we could take this ...
{'a': 1, 'b': 2, 'c': 3}

// ... and turn it into this ...
['a', 1, 'b', 2, 'c', 3]
// ... or even into this
"'a' 1 'b' 2 'c' 3"

// Or take this
await chargeCustomer()
if (purchased()) {
  await sendNotification()
}

// And turn it into this
if (purchased()) {
  await chargeCustomer()
  await sendNotification()
}
```

The downside is coding without a structural editor is awful. You're code often ends up looking like the following:
```clojure
(defn [arg1]
  (if (send-email-notification?)
    (try (send-email [(customer-email)])
      (catch Exception e
        (log/error
          (str "could not send email. Error: "
          (.toString e)))))
    (let [phone (customer-phone)]
      (if (sms-enrolled? phone)
        (send-sms [phone])))))
```

That's a lot of closing parenthesis next to each other. It can get worse though. Clojure doesn't exclusively use parenthesis (it uses brackets for vectors and curly braces for maps/sets), you can often end up with a mix of closing brackets, braces, and parenthesis. Rainbow brackets help (which I also setup), but it's still really hard to make sure that there are the right number of ending parenthesis. It's also why structural editing is so important for Lisps. Structural editing removes the need for developers to manually move around those closing and starting parenthesis. It allows developers to rearrange code without ever having to delete or insert new parenthesis by hand. IntelliJ comes with a great structural editor built-in. The Emacs community has built several great ones. I struggled to find a good one for NeoVim.

I first tried `vim-sexp` (and a mappings plugin for it), but I was disappointed that it didn't handle string termination properly. I then tried `paredit` (which is what I used for Emacs), but the port that I got working was buggy and would screw up my code whenever I yanked or pasted - including when it was just a single token. I had to add shortcuts to turn off my paredit, copy whatever string/function name/if statement that I needed, paste it, fix any parenthesis mistakes I made, and then turn on paredit. If I forgot any one of those steps, I'd often screw up my code and not notice for a good five to ten minutes (it's really hard to keep the parenthesis correct at the end of a function when you're looking at a nested if statement near the start). Thank goodness I had rainbow brackets, otherwise I'd never be able to fix my code.

In the end, Vim was a sub-par (and often frustrating) experience for Clojure/Lisp code. I'm sure that if I was working with someone who had a solid Vim setup for Clojure, they could help me figure out what was going on. However, I don't know people who are still using Vim for Lisp. When I had previously used Clojure, the developer who helped me get setup used Emacs. Emacs feels like it has better tooling overall for Lisp than Vim, which makes sense. First, Lisp is a very niche language family, so most developers and tooling aren't going to be for Lisp. It also has a different editing pattern than most languages with semantic editing, so  Emacs is scripted in a Lisp, so Emacs developers have to edit Lisp all the time, and given the frequency of editing Lisp they 

Also, I had issues with the REPL integration where sometimes sending a file/buffer to the REPL wouldn't work inside NeoVim. Usually a REPL restart fixed it, but it was super irritating. I didn't have any issues like that with IntelliJ, and I can't recall issues like that with Emacs.

## Getting used to "Syntax"

I also had to start getting used Clojure's "syntax" again (or as some Lisp developers like to say, the "lack of syntax").

Different programming languages fall into different syntax families. These language families make it so languages look vaguely similar if you squint just right. Here are a few examples of different language families:

```c
int foo(int c) {
  printf("hello\n");
  return c + 1;
}

foo(2);
```

```python
def foo(c):
  print("hello")
  return c + 1

foo(2)
```

```haskell
foo :: Int -> Int
foo c = c + 1

foo 2
```

```erlang
foo(c) when c < 0 ->
  io:format("hello~n"),
  c + 1;
foo(c) ->
  c + 1.

foo(2).
```

Those are just a few different syntax families. We have different ways of indicating types (or lack of types), statements vs expressions, blocks, end of expressions, function calls, etc.

In some of the language families the presence of parenthesis indicates a function call while a keyword indicates a function definition. Sometimes braces are used to indicate code blocks, other times it's indentation or the use of commas. Sometimes we use arrows, other times colons, etc. These different pieces of syntax are visual indicators telling the programmer about the semantics and organization of the code. These visual indicators allow for the developer to reason about the code. Lisps don't have that. Here's a foobar implementation in common lisp (indentation added for clarity):

```lisp
(defmethod foobar-impl ((n number))
  (cond
    ((= (mod n 15) 0) (print "FooBar"))
    ((= (mod n 5) 0) (print "Bar"))
    ((= (mod n 3) 0) (print "Foo"))
    (t (print n))))
  
(defmethod foobar (v)
  (map nil 'foobar-impl v))
```

There's not a lot of visual indicators. There's just lists of tokens inside lists of tokens inside more lists of tokens. That's the point of Lisp. It's all just LISt Processing. Lisp interpreters take nested lists of code and processes those lists. Clojure does add some syntax which helps, but not much. Here's the Clojure equivalent:

```clojure
(defn foobar [n]
  (cond
    (zero? (mod n 15)) "FooBar"
    (zero? (mod n 5)) "Bar"
    (zero? (mod n 3)) "Foo"
    :else (str n)))

(defn foobar-all [s]
  (map foobar s))
```

There are still not a lot of visual indicators. Almost everything is wrapped inside of parenthesis. We have parenthesis around statements, function definitions, conditions, and code blocks. This is what is meant by there being "no syntax." There are very few (if any) distinctive visual indicators built into the language.

Fortunately in Clojure, we still have _some_ syntax, _some_ indicators. These indicators exist entirely around data, which is fascinating since it puts the focus of the language and code on data as well.

### Clojure and Data

Clojure is very data focused. Data in Clojure is (usually) immutable, and a lot of effort has been put into making data structures somewhat performant. There's also a ton of options for data structures in Clojure, and everything gets it's own syntax.

Want a linked list? That's done with parenthesis prefixed with a quote (or calling the `list` function).

```clojure
;; Non-escaping quote
'(1 2 3)
;; escaping quote
`(1 2 ~b)
;; function
(list 1 2 3)
```

Want a vector? That's done with square brackets.

```clojure
[1 2 3]
```

Want a map? That's done with curly braces.

```clojure
{:a 1 :b 2}
```

Want a set? Add a `#` in front of your curly braces.

```clojure
#{1 2 3}
```

Want a character? A string? Or a keyword? There's syntax for those too.
```clojure
\a      ;; Character
"hello" ;; String
:hello  ;; Keyword
```

Do you have data that is mutable? What about data that may not be ready yet? Well there's syntax there too.

```clojure
;; Atoms are values that can mutate
;; They can also be safely shared across threads
;; Atoms do have implicit locks, so getting a value does block
(def abc (atom 3))

;; Some libraries use futures which may not have a value right away
;; Waiting for a future to finish will block
(def future-value (http/get "example.com"))

;; The @ says "get the underlying value even if I have to block the thread"
(print @abc)
(print @future-value)
```

Code can even be data - which is how macros work. Macros just take your lists of tokens as input, and outputs a new lists of tokens. Here's the syntax for that:

```clojure
'a             ;; Stores a symbol
'(+ a b c)     ;; Stores an expression
`(+ a b c)     ;; Stores an expression, but also resolves aliased imports
`(+ ~a ~@rest) ;; Grabs the values of a and rest, and then inserts those into a stored expression
```

Destructuring data also has syntax which mimics the syntax of the data type being destructured.

```clojure
(defn destructure
  [[item1 item2 & rest]
   {:keys [a b c] :as my-map]]
   (println (+ a b c item1 item2)))
```

The choice to add distinct syntax around data and data processing is incredibly impactful. It doesn't just impact the language design, but it also impacts the types of libraries written, how the community operates, and how developers write their application code.

For instance, maps and keywords are both functions in Clojure.
Nil (Clojure's null) is handled more gracefully at the language level.

```clojure
(def a-map {:a 2 :b 3 :c 4})

(:a a-map) ;; 2
(a-map :c) ;; 4
(:d a-map) ;; nil
(:e nil)   ;; nil
```

This means that developers will prioritize using keywords instead of strings for their maps. It also means that maps will often replace functions and if-else statements. This also means developers start reaching for data structures before they reach for code, so you get libraries like [Reitit](https://github.com/metosin/reitit) which does application routing using just data structures.


## The Data's Journey

As I continued working with Clojure, I started to think less about the code doing things or the systems I was building. In a lot of code bases, classes and functions will build a system of logic, and that system will take input and create output. There may be some form of "data" as input, but most of the time that data is transformed into a system (i.e. object) and that system is used or transformed. At the end of the day, the systems formed by code does the problem solving rather than any of the inputs.

With Clojure's focus on data, the focus on systems began to disappear. I became more focused on the data itself, and trying to adapt the data to become the solution. If I was given a vector of keywords and I needed the frequency count, I'd turn it into a map of keywords to numbers. If I then needed to get the keywords by frequency, I could invert the map and have collisions resolve into a vector. Or I could get the frequency values of the map and put them in a set to get unique frequency counts. At each step I focused on the journey data would take to solve the problem rather than building a system that solved the problem.

I also added journey markers in my code through comment blocks. These comment blocks made REPL evaluation easy since I could use semantic editing to select and evaluate only the contents of the comment block in the REPL. This allowed me to quickly test and debug code, and it also documented the shape of the data each function expected.

```clojure
(comment (foobar-all
          [1 2 3 4 5 6 10 12 14 15 16]))
```

Once my methods became more concrete, I could add "types" by defining a specification. These specifications described the parameters, and they could be used to generate automatic property tests. By default, spec enforcement is opt-in, which means they can be used for testing and debug builds without slowing down release builds (specs are runtime type checks).

```clojure
(s/fdef foobar
  {:args (s/cat :n int?)
   :ret string?})

(deftest foobar-test
  (is (empty? (:failure (stest/check `foobar)))))
```

I could also add pre and post validation checks that would be ran at runtime. These functioned as asserts to make sure that my parameters and/or return values met certain conditions. In theory, they could also make assertions about the program state (e.g. is there a database available?).

```clojure
(defn foobar-checked [n]
  {:pre [(pos? n) (int? n)]
   ;; The % is a standin for the return value
   :post [(or (= "Foo" %)
              (= "FooBar" %)
              (= "Bar" %)
              (= (str n) %))]}
  (foobar n))
```

## The REPL

Lisps have some really unique developer tools which I always miss when I move back to another language.
The first, and most talked about, is the REPL.

Lisp REPLs can be typed into to run code. However, there's been a lot of work added into REPLs, especially nREPL. With nREPL, you can not only connect to local and remote machines, but from your editor you can execute snippets from a file, execute the entire file, or even (re)load an entire project with just a few shortcuts. REPLs become an extension of the editor and a replacement for the compiler when developing code.

The ability to send parts of a file also encourages including common REPL statements inside of your code (and by extension your version control). This is why Clojure has the `comment` expression. The `comment` expression is just a macro that replaces the contents with `nil`. However, since it's seen as valid code by semantic editors and IDEs, it means it can be manipulated like normal code and the contents can be sent to a REPL.

This allows developers to include usage examples, REPL-specific code, setup scripts, debugging tools, and even interactive tutorials inside the source code without impacting the final build.

```clojure
(defn add [x y]
  (+ x y))

(comment (add 3 4))
```

This is a huge development and communication gain. I can create complicated test input, keep it next to my code, do my debugging and development, check in any changes I make to my test input, and then have it as a documented example for when I come back 6 months later. It's awesome!

## The JVM

Clojure runs on the JVM, which is very much a double edged sword. On one hand, Clojure code can take advantage of any Java, Kotlin, Groovy, or Scala library. It can also compile itself into classes for interop with Java. It has access to the Java standard library. Java based infrastructure works with Clojure - which is a big win if you're trying to introduce a new language to a Java-based company. Plugins for popular compiler frameworks (e.g. Maven) exist so it can be integrated into existing build pipelines.

The downside is that it has all of the shortcomings of the JVM, and then some. Java is known for taking up a lot of memory. Clojure's immutable data structures only exacerbate the issue. Java's Date class is not great. A lot of attempts have been made to "fix" it with new libraries and dates, but that generally leads to many different date classes in a single code base. Keeping those straight with compile time type checking can already be challenging in some code bases. Adding Clojure to the mix with dynamic typing and optional runtime checks makes the problem way worse.

Clojure also intentionally doesn't provide a standard library for all things. Instead, it relies heavily on the host language to provide a standard library for most things (e.g. date/time, I/O, OS calls, etc). The belief is that normalizing interop with the host programming language is better than trying to hide the host language.

The "rely on the host" mentality becomes problematic when people try to port Clojure to a new host language, such as JavaScript (via ClojureScript). What then ends up happening is that there is no common core for libraries to depend on. So, Clojure libraries need to have explicit checks for any host language they support, and then change their code to interop correctly with each host language. This in turn means that the Clojure library ecosystem is prone to fracturing and having libraries which only run on a single host language (usually being JVM only, but sometimes they're JavaScript only). It also means that a lot of attempts to port Clojure to other languages struggle to gain any ground or traction (e.g. ClojureCLR, Clojerl).

Also, the JVM doesn't support tail call optimization for recursion, so Clojure has a bunch of hacks to circumvent the issue, including trampolining and a special loop macro. It's unfortunate since several other functional languages I use do have tail call optimization, and a lot of algorithms are just nicer to use recursion rather than a hack - even if it's a standardized hack.

## Refactoring

Refactoring is quite a bit of a pain.

Clojure code is really easy to write the first time. It's pretty hard to change without throwing everything away and rewriting it. And sometimes, literally everything needs to get thrown away.

Clojure is dynamically typed, which already makes refactoring harder. However, Clojure also really drinks the functional Kool-aid, which means it's really easy to end up with long chains of map, filter, and reduce with lots of tiny one-off functions. These functions can make it hard to keep track of the shape of the data at any given point. And to make matters worse, there aren't very many good debuggers since Clojure works by evaluating nested lists of tokens rather than single lines. It's like the problem some JavaScript debuggers are having where when you add a breakpoint to a line with a lambda the debugger will ask "Do you want to breakpoint this line or the lambda?" Except it's much worse in Clojure. And due to how a lot of built in "primitives" work, doing print debugging often requires the extra step of wrapping code in a do.

```clojure
(if (some-cond)
  ;; I can't just add a print statement here since
  ;; that would make the "if true" branch become the
  ;; "if false branch"
  (do-something-i-need-to-debug)
  (else-i-do-not-care-about))

(if (some-cond)
  ;; Instead, I need to wrap it in a "do" statement
  ;; so I can add my debug print statement
  (do
    (println "debug trace")
    (do-something-i-need-to-debug))
  (else-i-do-not-care-about))
```

Also, due to the prevalence of map, filter, and reduce, changing the shape of the data early on in the chain requires updating every method later down in the chain. This gets especially problematic when changing data types entirely. I've ran into issues where I realize that I need to convert a string to a map at the start of a chain so that I can access an additional piece of data at the end of the chain. Doing that one thing usually requires rewriting everything.

## Debugging

Clojure just doesn't have a good debugger. There are a lot of hacks I've seen to add "debugging" to Clojure, but they don't really work.

I pretty much had to rely on print debugging combined with the REPL and my test cases in comment blocks. Anything else I couldn't get to work reliably. I did find some "debugging" libraries which just make print debugging easier since I don't have to wrap my code in `do` statements all the time. But due to how those "debuggers" work, I have to be careful not to leave their special syntax in place or else I'd break my release build.

## Conclusion

Clojure is by far my favorite Lisp. It's got it's faults and shortcomings, but it does have a great focus on data.

That said, I probably won't be using it too much in the near future. I'm trying to re-explore several functional programming languages, and I chose Clojure as the first one since it was my first main functional language, and it was the one I used the longest (mostly since I could get it running on the Java infrastructure at work).

Since I first started to use Clojure there have been a few other Lisps come out or that I've seen,such as Janet and Ferret. Both Janet and Ferret seem to be inspired by Clojure, but they are their own languages with more of a focus on speed and embedding rather than compatibility with enterprise software.

I also have taken small looks at other types of functional languages, such as Haskell, Erlang, Elixir, ReasonML/OCaml, Elm, and F\#. Haskell, ReasonML/OCaml, Elm, and F\# have beautiful type systems and they feel like they're trying to prevent issues at compile time. I can appreciate the beauty of those languages, and at some point I'll probably revisit them. However, compile time prevention of errors can only go so far. Production systems are prone to break, and I've had many breakages that a compiler just couldn't catch (out of disk, out of memory, network failure, cache being ignored, cache being outdated, network segmentation causing data replication, corrupted gzip files, etc). I'm now less interested in preventing failures because I know they will happen regardless. Instead I'm more interested in recovering from failures, so the next langauges I'm going to revisit are Erlang and Elixir.

Erlang and Elixir are dynamically typed like Clojure, so there's less of a focus on catching bugs at compile time. There is a new language called Gleam which is typed and works with Erlang/Elixir. However, I'm not going to use Gleam yet since it's a little too bleeding edge for what I'm trying to learn. I want to focus on the failure recovery and reliability mechanisms of the Erlang ecosystem. For this foray, I want to be using a language that's well documented and which has several books built on years (or even decades) of experiences on how to make systems reliable using the Erlang ecosystem. While Gleam is a great language, it just hasn't been around long enough to have the knowledge base I'm looking for. Once I gain that knowledge, I'll probably look at Gleam and apply what I've learned.

