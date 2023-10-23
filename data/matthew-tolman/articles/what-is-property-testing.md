Property testing is a developmental approach to writing tests (both unit tests and integration tests) which
helps developers find the limitations of their software. It isn't an all-encompassing set of principles
or practices which impacts the whole development lifecycle, unlike Test Driven Development. Instead, it's isolated to
the testing process, and it focuses on procedurally generating test inputs and then checking the properties of the output,
rather than relying on a predefined list of "Input X gives Output Y."

By doing procedural test input generation, property tests can give far greater coverage of edge cases, and it can find
more bugs than simply meeting a "line coverage" and "branch coverage" metric.

The procedurally generated inputs cover a far wider space than inputs manually created by developers. The wider input
space can catch edge cases developers may not be aware of or may not think to test. Edge cases can include
exposing integer overflow issues in arithmetic, divide by zeros, missing data relationships (e.g. partially deleted
records), inability to handle a full RFC spec (e.g. IPv6 hosts in SMTP^[smtp-ipv6]), or just bad logic.

Not all of these issues discovered by procedurally generated inputs need to be "fixed" (e.g. maybe you only need 42 bits
of precision, so arithmetic overflows at 50-bits of data can be ignored). Other times, you may just filter out data your
system won't handle (e.g. filter out IPv6 hosts from email addresses). But, some of the issues found are legitimate
issues that may have gone undetected until there was a production incident. These types of issues can go undiscovered
despite PR reviews and traditional developer-created example tests, even when developer tests get 100% line and branch
coverage metrics. Property testing becomes even more beneficial when there are "hidden" branches in code - something
that's common with code that throws exceptions (especially type errors) at runtime.

This article aims to be an introduction to property testing. I will also introduce a property testing framework which
I have written in (JavaScript/TypeScript)[https://gitlab.com/mtolman/js-prop-tests/]. I also wrote a framework in (C++)[https://gitlab.com/mtolman/c-property-testing]. In a future article I will discuss how I made my
framework, some of the design decisions (and mistakes) I made, and what the future holds for those property testing
frameworks (including potential ports to other languages).

But for now, let's take a look at how most developers typically write their tests, examine the pitfalls commonly
experienced, and see how property testing can help. I'm going to use a fairly complex example (at least more complex
than you traditionally get in a blog post) as a demonstration, mostly since the other blog posts I've seen on
property testing use examples so trivial they don't properly demonstrate the utility of property testing, or even how
to think with property tests.

[toc]

## The Example Problem

For this example, we are working on time tracking software. Our clients are businesses, and they use our software to track
how long each of their employees work, that way they can issue paychecks and issue invoices. Because our clients issue
invoices, they want to be able to track the hours worked by both salaried and hourly employees, even if the hours worked
doesn't impact a salaried employee's paycheck. And, they'd also like us to report the paycheck amount of salaried employees,
that way they only have to use one tool to calculate paychecks for all of their employees.

Our clients (the businesses) would like to have some reports where, for any given week, they could see the total time
worked by all employees, the total wages owed for that week, and see a breakout of these numbers by employee. Our report
will only run on a week-by-week basis. We aren't doing any fancy "run it monthly" or "run it for the last 15 days". We'll
also keep the calendar math simple and assume there are always 52 weeks in a year.

For simplicity, I'm going to assume a [[article:separation-of-logic-from-data-storage]] system. This means we won't be
making database calls inside our report code. Instead, we'll be passing in the data we're going to process, and we're
going to output the result. The input data would look something like the following JSON:

```json
{
  "employees": [
    {
      "id": "123942",
      "name": "Jane Doe",
      "hourlyWage": 11.50
    },
    {
      "id": "9857847",
      "name": "Jack Dorsey",
      "yearlySalary":  200000
    }
  ],
  "timesheets": [
    {
      "id": "19230",
      "startEpoch": 1674223351000,
      "endEpoch": 1674266551000,
      "employeeId": "123942"
    },
    {
      "id": "4958034",
      "startEpoch": 1674223351000,
      "endEpoch": null,
      "employeeId": "9857847"
    }
  ]
}
```

In the above data, you'll notice that we have both salaried and hourly workers. Hourly workers have an hourly rate
(we're ignoring overtime rules for this example). Meanwhile, salaried workers have just their yearly salary.

You'll also notice that there are some timesheets without an ending epoch. These are for timesheets where the employee
is still clocked in. For our report, we'll just filter out those timesheets.

As for the start and end times, we're using the UTC epochs in milliseconds (since JavaScript's `Date` class works in
milliseconds). We're also just using normal JavaScript numbers for the wages and hours. The numbers in JavaScript are
doubles which can introduce weird rounding bugs (e.g. `0.1 + 0.2`). However, this is a blog post about property testing,
so we aren't going to worry about doing correct financial calculations.

Our IDs are also all strings, even though they are numeric. There are many reasons this would happen in a real-world
code base(e.g. wanting to use 64-bit integer ids, planning a migration to a UUID, etc.). However, in this case it's
just so that I can make sure all object keys are strings, and that they stay with the same data type inside the object.
I just didn't want to deal with any implicit number to string conversion.

For this article, we'll be writing our code in JavaScript (using Node.js to run it). We'll be using the testing
library [Jest](https://jestjs.io/) and [my own property testing library](https://www.npmjs.com/package/@matttolman/prop-test).
We'll also use the date library [DateFns](https://date-fns.org/). This means that all you need to do to follow along is
to run the following:

```bash
mkdir prop-test-example
cd prop-test-example
npm init -y
npm i -D jest @matttolman/prop-test
npm i -S date-fns
```

### Writing the code (first draft)

Out goal for the report is to generate an object with a "totals" object which has the total time and wages worked. We'll
also have a "byEmployees" object which has all the time and wages worked by employee. The "byEmployees" object will use
the employee id as the key. We aren't doing any fancy calculations, just addition and multiplication. I'll put the code
in a file called `code.js`.

First, we need to extract our employee and timesheet data from the input, as well as initialize our data accumulators.

```js
// code.js
const dateFns = require('date-fns')

function makeReport(reportData) {
    const {employees, timesheets} = reportData
    const employeeStats = {}
    const totalStats = {
        timeWorked: 0,
        wagesDue: 0,
    }
```

Next we need to iterate over all of our timesheets and filter out any which are still "on the clock."

```js
    for (const timesheet of timesheets) {
        // If there's no end time, then we're still on the clock
        // and can't report on the timesheet yet
        if (!timesheet.endEpoch) {
            continue
        }
```

We then need to find the employee tied to the timesheet. We'll also grab their accumulated data (or initialize it if
it doesn't exist).

```js
        const employeeId = timesheet.employeeId
        const employee = employees.find(
            employee => employee.id === employeeId
        )

        employeeStats[employee.id] = employeeStats[employee.id] || {
            name: employee.name,
            id: employee.id,
            timeWorked: 0,
            wagesDue: 0,
        }
```

Next we need to get the number of hours the employee worked. Since we've already filtered out any dates with a null end date, we can
get our start and end date epochs. We'll use DateFns to calculate the number of hours worked.

```js
        const start = new Date(timesheet.startEpoch)
        const end = new Date(timesheet.endEpoch)
        const hours = dateFns.differenceInHours(end, start)
        employeeStats[employee.id].timeWorked += hours
        totalStats.timeWorked += hours
```

Next we can calculate the wages and add them to our stats as well. For any given week, we will add the weekly salary
of the yearly employees and the hourly wage of the hourly employees.

```js

        const wagesDue = (employee.yearlySalary) ? employee.yearlySalary / 52
                : hours * employee.hourlyWage;
        employeeStats[employee.id].wagesDue += wagesDue
        totalStats.wagesDue += wagesDue
    }
```

After that we just have to return our results.

```js
    return {
        totals: totalStats,
        byEmployee: Object.values(employeeStats)
    }
}
```

Here's the full code:

```js
function makeReport(reportData) {
    const {employees, timesheets} = reportData
    const employeeStats = {}
    const totalStats = {
        timeWorked: 0,
        wagesDue: 0,
    }

    for (const timesheet of timesheets) {
        if (!timesheet.endEpoch) {
            continue
        }
        const employeeId = timesheet.employeeId
        const employee = employees.find(
            employee => employee.id === employeeId
        )

        employeeStats[employee.id] = employeeStats[employee.id] || {
            timeWorked: 0,
            wagesDue: 0,
        }

        const start = new Date(timesheet.startEpoch)
        const end = new Date(timesheet.endEpoch)
        const hours = dateFns.differenceInHours(end, start)
        employeeStats[employee.id].timeWorked += hours
        totalStats.timeWorked += hours

        const wagesDue = (employee.yearlySalary) ? employee.yearlySalary / 52
                : hours * employee.hourlyWage;
        employeeStats[employee.id].wagesDue = wagesDue
        totalStats.wagesDue += wagesDue
    }

    return {
        totals: totalStats,
        byEmployee: Object.values(employeeStats)
    }
}

module.exports = makeReport
```

## Traditional Unit Tests (Example Based)

Now it's time to test. For TDD, we would have written the tests before the code. However, for this post we aren't
doing TDD (maybe sometime I'll write a post sharing my thoughts on TDD). Instead, we wrote the code first, and then we
wrote the tests. This is generally the process that I find most developers follow at most places, and it does well enough.

In our hypothetical example, our company measures test quality with line and branch coverage. It just uses the built-in
coverage tool for Jest. This is a pretty typical way to measure test health, even if it has some inaccuracies (which we'll
see soon enough).

The first test we'll write is simple. Without any employee or timesheet data, we should get back zeroes. Let's write that test:

```js
const makeReport = require('./code')

describe('make report', () => {
    it('handles empty data', () => {
        expect(makeReport({employees: [], timesheets: []}))
            .toEqual({
                totals: {timeWorked: 0, wagesDue: 0},
                byEmployee: []
            })
    })
})
```

If we run `npx jest --collectCoverage`, we'll see the following:

```text
 PASS  ./code.spec.js
  make report
    ✓ handles empty data (3 ms)

----------|---------|----------|---------|---------|-------------------
File      | % Stmts | % Branch | % Funcs | % Lines | Uncovered Line #s 
----------|---------|----------|---------|---------|-------------------
All files |   33.33 |        0 |      50 |      35 |                   
 code.js  |   33.33 |        0 |      50 |      35 | 12-31             
----------|---------|----------|---------|---------|-------------------
Test Suites: 1 passed, 1 total
Tests:       1 passed, 1 total
Snapshots:   0 total
Time:        0.711 s, estimated 1 s
Ran all test suites.
```

Already we're at 35% line coverage. We should be able to improve that quite a bit by adding a few more tests.

Two more tests come to mind pretty easily. We can add a test for calculating hourly wage numbers, and another one for
calculating salaried wages.

Fortunately, our input data specification included some example data for hourly and salaried employees, so we'll just
copy that data into our tests. That way, we know if we've met the original specification or not.

```js
it('handles hourly wages', () => {
        expect(makeReport({
            employees: [
                {
                    id: "123942",
                    name: "Jane Doe",
                    hourlyWage: 11.50
                },
            ],
            timesheets: [
                {
                    id: "19230",
                    // This is a time period of 12 hours
                    startEpoch: 1674223351000,
                    endEpoch: 1674266551000,
                    employeeId: "123942"
                },
            ]
        }))
            .toEqual({
                totals: {timeWorked: 12, wagesDue: 12 * 11.50},
                byEmployee: [
                    {
                        name: "Jane Doe",
                        id: "123942",
                        timeWorked: 12,
                        wagesDue: 12 * 11.50
                    }
                ]
            })
    })

    it('handles yearly workers', () => {
        expect(makeReport({
            employees: [
                {
                    "id": "9857847",
                    "name": "Jack Dorsey",
                    "yearlySalary":  200000
                }
            ],
            timesheets: [
                {
                    id: "4958034",
                    // This is a time period of 12 hours
                    startEpoch: 1674223351000,
                    endEpoch: 1674266551000,
                    employeeId: "9857847"
                }
            ]
        }))
            .toEqual({
                totals: {
                    timeWorked: 12,
                    wagesDue: 200000 / 52
                },
                byEmployee: [
                    {
                        name: "Jack Dorsey",
                        id: "9857847",
                        timeWorked: 12,
                        wagesDue: 200000 / 52
                    }
                ]
            })
    })
```

Now let's check our code coverage by running `npx jest --collectCoverage` again. Here's the output:

```text
 PASS  ./code.spec.js
  make report
    ✓ handles empty data (3 ms)
    ✓ handles hourly wages (1 ms)
    ✓ handles yearly workers (1 ms)

----------|---------|----------|---------|---------|-------------------
File      | % Stmts | % Branch | % Funcs | % Lines | Uncovered Line #s 
----------|---------|----------|---------|---------|-------------------
All files |   95.23 |    83.33 |     100 |      95 |                   
 code.js  |   95.23 |    83.33 |     100 |      95 | 13                
----------|---------|----------|---------|---------|-------------------
Test Suites: 1 passed, 1 total
Tests:       3 passed, 3 total
Snapshots:   0 total
Time:        0.921 s, estimated 1 s
Ran all test suites.
```

There's only one line not covered, and that's the filter for when we don't have an end epoch. We can quickly add in a test
and get our coveted 100% coverage. Here's the new test:

```js
    it('handles timesheets without ending epochs', () => {
        expect(makeReport({
            employees: [
                {
                    id: "9857847",
                    name: "Jack Dorsey",
                    yearlySalary:  200000
                }
            ],
            timesheets: [
                {
                    id: "4958034",
                    // This is a time period of 12 hours
                    startEpoch: 1674223351000,
                    endEpoch: null,
                    employeeId: "9857847"
                }
            ]
        }))
            .toEqual({
                totals: {
                    timeWorked: 0,
                    wagesDue: 0
                },
                byEmployee: []
            })
    })
```

Now let's recollect our code coverage by running `npx jest --collectCoverage` again. Here's the output:

```text
 PASS  ./example.spec.js
  make report
    ✓ handles empty data (3 ms)
    ✓ handles hourly wages (1 ms)
    ✓ handles yearly workers
    ✓ handles timesheets without ending epochs (1 ms)

----------|---------|----------|---------|---------|-------------------
File      | % Stmts | % Branch | % Funcs | % Lines | Uncovered Line #s 
----------|---------|----------|---------|---------|-------------------
All files |     100 |      100 |     100 |     100 |                   
 code.js  |     100 |      100 |     100 |     100 |                   
----------|---------|----------|---------|---------|-------------------
Test Suites: 1 passed, 1 total
Tests:       4 passed, 4 total
Snapshots:   0 total
Time:        0.645 s, estimated 1 s
Ran all test suites.
```

We now have 100% test coverage! And, our tests are derived from our input specification, so we know it handles the spec.

We ship our code out to production. Everything goes well, and we move onto other features.

### False assurance

After a while, two bug reports come in for our timesheet report. Here's the first one:

> **Timesheet Report Fails**
> 
> ```text
> Business Account: 5932984934
> Environment: Production
> Report Dates: 10/22/2023 - 10/29/2023
> Transaction Id: 493283029138293293282
> Log Error Message:
>   TypeError: Cannot read properties of undefined (reading 'id')
> Description:
> 
> An employee had resigned. Before then reports worked fine.
> Now they don't work anymore. Please advise.
> ```

 Here's the second one:

> **Timesheet Report Wrong for Some Employees**
> 
> ```text
> Business Account: 46959348204
> Environment: Production
> Report Dates: 10/22/2023 - 10/29/2023
> Transaction Id: 349593429394084832
> Log Error Message: None
> Description:
> 
> Employee Rob Martian is getting inaccurate wage numbers
> in the report. Please investigate.
> ```

Before we investigate what these errors are, let's first think about why we got them. We had 100% test coverage. We
covered every line of code. We hit every branch. We matched the specification. We shouldn't be getting a report failure,
and we shouldn't be getting behavior that wasn't covered by the specification. Right? 

Well, the answer is that showing code is correct through tests is not about coverage. It's not about writing tests before
or after writing the actual code. It's not about having a massive test suite that's several magnitudes bigger than the
code being tested.

Tests that rely on examples never prove that code is correct. It only shows that it handles cases the developer
thought of, and which the developer thought would be easy enough to write a test for.

Sometimes, it's even worse than that. Automated tests are the developer picking examples which confirms their own biases
and assumptions. If a developer never writes a try/catch in their code, then they are generally assuming that either the
code will never fail, or that someone else will handle the failure. If a developer doesn't write data validation, they
are assuming that piece of code will never get invalid data. Consequently, we can reasonably expect (and often do see)
that those developers don't write tests which could cause their code to fail or pass-in invalid data. Consequently, our
test suite will not catch those edge cases.

That's not to say that example-based testing is inherently bad. It's good to know what assumptions the developer was
operating with, and those assumptions are encoded into the tests (or, more appropriately, the missing tests). However,
example-based tests are terrible for finding bugs and proving correctness. And that's where property tests start to
come in.

## Property Based Testing

The goal of property-based tests is to no longer have the developer be solely in-charge of writing examples. Instead, we
start outsourcing some of the example writing to the computer, and we see if the computer can find any issues with our
code.

That said, computers aren't very smart. They don't know how to accurately read code, understand what it does, and then
write tests that show the limitations of that code. However, they are really good at one thing: they can brute force a
lot of computations.

When we have the computer come up with example test data, we don't just have it come up with a handful of examples.
Instead, we have it come up with hundreds to thousands of examples and try to brute force a failure from our code. This
is the heart of property based testing. We tell the computer "here's a program, here's how to create input for that program,
and here's how the program should behave. Now try to break it." If the computer can't cause the program to misbehave, the
test passes. If the computer does cause the program to misbehave, the test fails.

### Defining Program Behavior

Telling a computer how to generate random data is fairly straightforward, and we'll cover it a little bit more later on.
However, telling the computer how to verify that a program worked is a lot harder.

With example-based tests, the answer is fairly easy. We have some input value, we have an expected output, and we just
check that running the program through the code returns the exact same output.

With property tests, we have a range of inputs that we feed into the program, which means we'll end up with a range of
valid outputs. The first hunch is to say "well, let's just make sure the output values are in the range of all possible
outputs." However, the issue is that the range of all possible outputs could be so vague that it doesn't tell us if anything
is correct.

For example, let's consider defining the behavior of "integer addition." The range of all possible inputs is any two
integers we can represent on our computer. The range of all possible outputs is also the range of all integers we can
represent on our computer. Below is some psuedocode:

```js
function add(x, y) { /* mystery code here */ }

describe('add', () => {
    expect(add(any_int(), any_int())).toBe(a_valid_int)
})
```

The issue with the above code is it doesn't actually tell us if we've done addition, or subtraction, or multiplication,
or anything else. All the following add methods pass the above check:

```js
function add(x, y) { return x + y; }
function add(x, y) { return x - y; }
function add(x, y) { return x * y; }
function add(x, y) { return x > y ? x : y; }
function add(x, y) { return 1; }
```

Instead of just thinking about the range of all valid outputs, we need to think about the common properties of those
valid outputs. It's this shift of thinking that can take a while, so we'll walk through it step-by-step with the addition
problem before we dive back into our full report problem.

One of the best ways to figure out the output properties is to think about the relationships of the inputs. Here's a few
things to consider:
* Does the order of the inputs matter (i.e. is`fn(x, y) !== fn(y, x)`)?
* Can you chain the outputs as an input (i.e. is `fn(fn(x, y), z)` valid?
  * If so, does it matter how you chain the inputs (i.e. is `fn(fn(x, y), z) !== fn(x, fn(y, z))`))?
* Is there a set of inputs where the output will match one or more inputs (i.e. can `fn(x, y) === x`)?
* Does the input put any constraints on the output? (e.g. with `new Array(n)` the input `n` constrains the size of the output array)
* Is there input that can cause an exception to be thrown?

For addition, we can run through those above questions and come out with some properties.

* Does the order of the inputs matter (i.e. is `fn(x, y) !== fn(y, x)`)?
  * No. Addition is Commutative so `add(x, y) === add(y, x)`
* Can you chain the outputs as an input (i.e. is `fn(fn(x, y), z)` valid?
    * Yes, `add(add(x,y), z)`
  * If so, does it matter how you chain the inputs (i.e. is `fn(fn(x, y), z) !== fn(x, fn(y, z))`))?
    * No, `add(add(x, y),z) === add(x, add(y, z))`
* Is there a set of inputs where the output will match one or more inputs (i.e. can `fn(x, y) === x`)?
  * Yes, `add(x, 0) === x`
* Does the input put any constraints on the output? (e.g. with `new Array(n)` the input `n` constrains the size of the output array)
  * No. Addition of any two whole numbers does not guarantee the output will be bigger or smaller (`-4 + -2` is smaller, but `4 + 2` is bigger, and `-4, 5` is in-between)
* Is there input that can cause an exception to be thrown?
  * No

The above questions gave us three different properties to check for. These properties also happen to have names^[addition-properties].
The names are Commutative, Associative, and Identity. There also are other properties of addition I've seen listed (e.g.
Distributive when used with multiplication, Successor for `y = 1`, etc.). But the three properties Commutative, Associative,
and Identity are the most common properties I've consistently seen, so we'll just stick with those for this post.

Using [@matttolman/prop-test](https://www.npmjs.com/package/@matttolman/prop-test) we can codify our properties as follows:

```js
const {property, generators} = require('@matttolman/prop-test')

function add(x, y) { /* some mysterious code */ }

describe('JavaScript', () => {
    it('can add numbers', () => {
        const intGen = generators.number.ints()
        property('commutative', intGen, intGen)
            .test((a, b) => {
                expect(add(a, b)).toEqual(add(b, a))
            })

        property('associative', intGen, intGen, intGen)
            .test((a, b, c) => {
                expect(add(add(a, b), c)).toEqual(add(a, add(b, c)))
            })

        property('identity', intGen)
            .test((a) => {
                expect(add(a, 0)).toEqual(a)
            })
    })
})
```

> Note: In the above I'm limiting to just integers. This is since floating point numbers have issues^ where
> they can violate the mathematical properties of addition[floating-point-errors], so we need to use ints to avoid this.
> One neat thing is that we don't have to limit ourselves to a "safe" range for the properties to hold. Our properties
> for addition are independent of whether we hit an integer overflow or exceed our safe integer range, so we don't need
> to limit the range.

We can now run all of our different add methods, and we'll get only one that works (the method that actually adds). If
we try to run an add method that doesn't work (e.g. `function add(x,y) { return x - y }`), then we'll get ouptut similar
to the following:

```text
 FAIL  ./test.spec.js
  Math
    ✕ can add (883 ms)

  ● Math › can add

    Property Failed: associative
    ------------------------------------------------
    Seed: 1975078912
    Original Input: [-503,-128,-193]
    Simplified Input: [-333,-333,-333]
    ------------------------------------------------

    expect(received).toEqual(expected) // deep equality

    Expected: -438
    Received: -824
```

You'll notice a few things going on with the error message. The first is that while the error is a Jest error, there is a bunch
of extra stuff added in. That extra stuff is my property test framework at work. It's telling us which property failed,
the inputs which made it fail, and a "simplified" input which also fails (the simplified input is usually something
like smaller arrays, smaller numbers, matching numbers, etc.). We also get a seed which is the seed used for creating
the random values. This seed can be set in the environment variable `PROP_TEST_SEED` to replay the failing test run.

The outputted values (the inputs and the seed) allow us to start debugging our code to see what went wrong and why.
They also allow us to create a new test case with that failed example to prevent regressions. For instance, we could
add the following test case to prevent a regression with the original inputs:

```js
    it('passes regression test 1', () => {
        const a = -503
        const b = -128
        const c = -193
        expect(add(add(a, b), c)).toEqual(add(a, add(b, c)))
    })
```

Now we'll take a look at how to write tests to better check our time tracking report code.

## Improving the Testing of Our Report code

As a reminder, here's our report code:

```js
function makeReport(reportData) {
    const {employees, timesheets} = reportData
    const employeeStats = {}
    const totalStats = {
        timeWorked: 0,
        wagesDue: 0,
    }

    for (const timesheet of timesheets) {
        if (!timesheet.endEpoch) {
            continue
        }
        const employeeId = timesheet.employeeId
        const employee = employees.find(
            employee => employee.id === employeeId
        )

        employeeStats[employee.id] = employeeStats[employee.id]|| {
            name: employee.name,
            id: employee.id,
            timeWorked: 0,
            wagesDue: 0,
        }

        const start = new Date(timesheet.startEpoch)
        const end = new Date(timesheet.endEpoch)
        const hours = dateFns.differenceInHours(end, start)
        employeeStats[employee.id].timeWorked += hours
        totalStats.timeWorked += hours

        const wagesDue = (employee.yearlySalary) ? employee.yearlySalary / 52
                : hours * employee.hourlyWage;
        employeeStats[employee.id].wagesDue += wagesDue
        totalStats.wagesDue += wagesDue
    }

    return {
        totals: totalStats,
        byEmployee: Object.values(employeeStats)
    }
}
```

### Generating test data

To test our report code, we first need the computer to generate the following:
* Random employees
* Random timesheets

Our employees have the following fields:
* `name` - The employee's name
* `id` - The employee's id
* `hourlyWage` or `yearlySalary` - How much money the employee makes

Our timesheets have the following fields:
* `id` - Timesheet id
* `startEpoch` - Starting unix timestamp
* `endEpoch` - Ending unix timestamp or null
* `employeeId` - ID of the employee

To tell the computer how to generate the data, we'll tell it the general shape the data should have and any constraints
that the data should follow. After that, the computer will start generating lots of example data which we can pass in.
Fortunately, this isn't going to be too hard since my property testing framework provides a lot of built-in generators
that we can mix and match.

Let's start with the employee data generation:

```js
// Imports for all of our property tests
const {
    combine, just, map, oneOf, DataComplexity, array
} = require("@matttolman/prop-test/generator/common")
const {numeric} = require("@matttolman/prop-test/generator/text/ascii")
const {fullName} = require("@matttolman/prop-test/generator/person")
const {floats, ints} = require("@matttolman/prop-test/generator/number")
const dateFns = require('date-fns')
const {property} = require("@matttolman/prop-test");


// Define our hourly employees
const hourlyEmployeeGenerator = combine({
  id: numeric({minLen: 1, maxLen: 20}),
  name: fullName(),
  hourlyWage: floats({min: 10, max: 50})
})

// Define our salaried employess
const salaryEmployeeGenerator = combine({
  id: numeric({minLen: 1, maxLen: 20}),
  name: fullName(),
  yearlySalary: floats({min: 45000, max: 200000})
})

// Employees can be either hourly or salaried
const employeeGenerator = oneOf(
    hourlyEmployeeGenerator, salaryEmployeeGenerator
)
```

The `combine` method combines multiple data generators to create a single object. It handles calling all the sub-generators
automatically, and it also handles shrinking the result. The `numeric` generator creates a numeric ASCII string (which
is why it's imported from the `ascii` package). The `fullName` generator generates a random first name, last name combo.
And the `floats` generator generates random floating point numbers.

The `oneOf` generator generates a value using one of the random sub-generators. Unlink `combine` which calls all sub-generators,
`oneOf` will randomly call only one sub generator and then directly use the output without combining it with anything else.

Next let's define our timesheet generator

```js
const timesheetGenerator = map(
        combine({
          employeeId: numeric({minLen: 1, maxLen: 20}),
          id: numeric({minLen: 1, maxLen: 20}),
          startEpoch: ints({min: 1674172800, max: 1674734400}),
          hours: oneOf(ints({min: 1, max: 24}), just(null)),
        }),
        (ts, seed) => {
          // Enforce that the end time never comes before the start time
          const hours = ts.hours
          if (hours) {
            ts.endEpoch = dateFns.addHours(
                (new Date(ts.startEpoch)),
                hours
            ).getTime()
          }
          else {
            ts.endEpoch = null
          }
          delete ts.hours
          return ts
        }
)
```

The `just` generator just returns a single value. Here it's returning `null`, that way we can get an ending hour or null.

The `map` generator transforms the result of a generator. The `map` generator allows us to add constraints to our result.
In this case, we are constraining our input so that the end time always comes so many hours after the start time. This
means we won't be testing for when our end time comes before the start time. It's generally a good idea to outline these
assumptions, that way if someone is wondering what we haven't tested yet, they can take a look at our generators.

To test out our generators, we can call the `create` method on them as follows:

```js
console.log('Hourly Employee', hourlyEmployeeGenerator.create(1))
console.log('Salaried Employee', salaryEmployeeGenerator.create(1))
console.log('Employee 1', employeeGenerator.create(1))
console.log('Employee 2', employeeGenerator.create(2))
console.log('Timesheet 1', timesheetGenerator.create(1))
console.log('Timesheet 2', timesheetGenerator.create(2))
```

We'll get output similar to the following:

```text
Hourly Employee { id: '1702726724650', name: 'Carlos Alberto Šťastná', hourlyWage: 10.121951531647227 }
Salaried Employee { id: '1702726724650', name: 'Carlos Alberto Šťastná', yearlySalary: 45472.56218513301 }
Employee 1 { id: '97860', name: 'Christer Magalhães', yearlySalary: 67589.48164237419 }
Employee 2 { id: '1702726724650', name: 'Carlos Alberto Šťastná', hourlyWage: 10.121951531647227 }
Timesheet { id: '1702726724650', employeeId: '862', startEpoch: 1674631705, endEpoch: null }
Timesheet { id: '1977398106104393', employeeId: '3', startEpoch: 1674483020, endEpoch: 1750083020 }
```

> You may have noticed that our names have non-ascii characters, and that they have more than just the two names most
> Americans would expect for full names (first/last). This is because, by default, @matttolman/prop-test library will
> pull in more than simple names. This includes European names with accents, names with multiple parts
> such as last names with two parts (e.g. Hernandez Gonzales), names with dashes (e.g. Filipe-Romero), names with
> apostrophes (e.g. O'Connor), names that I've seen with user-initiated hacks that I've seen in B2B
> software (e.g. Jack \[Acct\#155932]) and names that don't even use the latin alphabet (e.g. 志旼). You can adjust these
> settings with the minComplexity and maxComplexity parameters.
> 
> Example: `fullName(DataComplexity.RUDIMENTARY, DataComplexity.BASIC)`.

### Our first real property test

Now that we have our data generators, it's time to put them to use. Let's start by just pushing the data through and
see what we get. For now, we won't worry about constraints. We just want to see if things work.

```js
describe('makeReport', () => {
    it('handles random data', () => {
        property('does not crash')
            .withGens(
                array(employeeGenerator),
                array(timesheetGenerator)
            )
            .test((employees, timesheets) => {
                makeReport({employees, timesheets})
            })
    })
})
```

If we run our tests, we get a crash!

```js
Property Failed: does not crash
------------------------------------------------
Seed: ...
Original Input: [[], [{ id: '1977398106104393', employeeId: '3', startEpoch: 1674483020, endEpoch: 1750083020 }, ...]]
Simplified Input: [[], [{ id: '1977398106104393', employeeId: '3', startEpoch: 1674483020, endEpoch: 1750083020 }]]
------------------------------------------------

TypeError: Cannot read properties of undefined (reading 'id')
    at id (code.js:18:32)
```

Well, that didn't take long. We managed to reproduce our report failure bug report with our very first try. But why did
it crash? Well, let's take a look at the line of code and see what's going on.

```js
const employeeId = timesheet.employeeId
const employee = employees.find(
    employee => employee.id === employeeId
)

employeeStats[employee.id] = employeeStats[employee.id]|| {
    /* truncated for brevity */
}
```

What we're doing here is we're getting the employee id from the timesheet and seeing if there's an employee with that id.
We then grab the "id" property off of our employee without checking if our search was successful.

In other words, our code here assumes that we will always find an employee. But, if we didn't find an employee the code
fails. In this case, our employee list was empty, but our timesheet list wasn't, so we couldn't possibly find an employee

But, why did we get that error in production? Well if we look at the description again, it says "An employee had
resigned. Before then reports worked fine. Now they don't work anymore." In other words, an employee had resigned,
their record was deleted, and now when the business runs reports they're getting timesheets that are tied to an employee
which no longer exists.

> Here "deleted" could be a soft delete or a hard delete. Often queries filter out any record which was soft deleted.
> This means it's quite possible this same bug would appear in a soft-delete system because of a filter, even though
> the record still exists!

There are a few potential solutions here: filter out timesheets with missing employees, tie the timesheets to an "unknown"
employee, or only include the stats in the totals and don't provide an individual breakout. For this blog post, we'll just
filter out the timesheets. The following code does that:

```js
const employeeId = timesheet.employeeId
const employee = employees.find(
    employee => employee.id === employeeId
)

if (!employee) {
    continue
}
// rest of code goes here
```

Now if we rerun our tests, we'll see that they just hang. Well, they aren't really hanging, they're just taking a really
long time. That's because the default maximum size for generated arrays is 1024 elements. Obviously we have some sort of
performance issue, but right now we're trying to get our code correct before we make it fast. Let's make the maximum
array size a lot smaller.

```js
property('does not crash with possibly missing ids')
    .withGens(
        array(employeeGenerator, {maxLen: 10}),
        array(timesheetGenerator, {maxLen: 10})
    )
    .test((employees, timesheets) => {
        makeReport({employees, timesheets})
    })
```

Now our tests run pretty quick and they "pass". Let's check what our coverage looks like. We'll comment out our example
tests and just take a look at our property tests.

```js
 PASS  ./prop.spec.js
  makeReport
    ✓ handles random data (907 ms)

----------|---------|----------|---------|---------|-------------------
File      | % Stmts | % Branch | % Funcs | % Lines | Uncovered Line #s 
----------|---------|----------|---------|---------|-------------------
All files |     100 |      100 |     100 |     100 |                   
 code.js  |     100 |      100 |     100 |     100 |                   
----------|---------|----------|---------|---------|-------------------
Test Suites: 1 passed, 1 total
Tests:       1 passed, 1 total
Snapshots:   0 total
Time:        1.494 s, estimated 3 s
Ran all test suites.
```

The code is reporting full coverage, but how can that be? Well, a lot of it has to do with how I wrote the built-in
generators. The generators aren't designed to have high entropy (aka. produce truly "random" results). They're designed to
be extremely deterministic, almost to a fault. This means that, by chance, the computer can "accidentally" create an
employee and timesheet with correlated ids. And, because of how I wrote the code, that chance is pretty high.

> Note: depending on when you read this, I may have increased the entropy of the generators. If so, then the above
> statement may no longer be accurate, and you may be getting much less test coverage.

However, relying on me not changing the amount of entropy in my library isn't a good habit to develop. For one, I'm just a
random developer on the web making an open/public source project that I can change at any point in time. I can always
change my generation algorithms to have a higher amount of entropy, which could break your code and cause lots of
headaches for you (but not me). Second, most other property testing frameworks don't have entropy as low as my
framework. So, if you ever decide to use another system (or have to use another system), then you'll need to know
how to adapt. Fortunately, it's pretty easy to adapt by creating generators which add a correlation relationship
between the outputs of our generators.

> Just a note, we aren't going to delete this test that "checks nothing." It did find a bug, so we'll leave it in place.
> It may be helpful to rename the test to "make sure doesn't fail with random input" or something like that. Or, if you
> don't like leaving that property test, you can take the failing inputs and make an example-based test to check for
> that particular failure instead.

### Correlating Records

To correlate records, we're going to `map` the output from different generators. This mapping step will change the
generator output to add a relationshiop constraint. Below is an example which correlates employees and timesheets:

```js
// Create an employee and a bunch of timesheets
// tied to that employee.
// Taking the employee generator as an input
// since we have 3 different methods we will want to
// choose from
const employeeTimesheetGenerator = (makeEmployees = employeeGenerator) => map(
        // First we create our employee and timesheets
        combine({
          employee: makeEmployees,
          timesheets: array(timesheetGenerator, {maxLen: 5})
        }),
        ({employee, timesheets}) => {
            // next, we iterate over every timesheet and
            // update the employee id to match our employee's
            // id, that way it's tied to our employee 
            for (let i = 0; i < timesheets.length; ++i) {
                timesheets[i].employeeId = employee.id
            }
            return {employee, timesheets}
        }
)
```

While we're at it, we can also create a generator that makes lots of employees and timesheets, then formats those 
employees and timesheets into the data structure we need for out report funciton.

```js
const reportInputDataGenerator = (makeEmployees = employeeGenerator) => map(
        array(employeeTimesheetGenerator(makeEmployees), {maxLen: 5}),
        (employeeTimesheets) => {
          const result = {employees: [], timesheets: []}
          for (const employeeTimesheet of employeeTimesheets) {
            result.employees.push(employeeTimesheet.employee)
            result.timesheets.push(...employeeTimesheet.timesheets)
          }
          return result
        }
)
```

These two generators allow us to create test cases that for our related data. We're passing in which generator to use
so that we can test salaried vs hourly vs both.

### Writing Property Tests With Behavior Checks

Now comes the next part, which is writing the tests that actually check for some sort of behavior (other than if it
crashes or not). For this, we need to figure out the behavior we're looking for. These behaviors aren't looking for
exact values, but rather properties of our output.

For wages of hourly employees, we can check that the sum of the wages fall within a specific boundary. On the low end,
we have a minimum wage ($10), and a minimum number of hours for completed timesheets (1 hour). We also can check our
input to count the number of completed timesheets. Our total hourly paid wage cannot go below $10 * 1 hour * number of
completed timesheets. On the other end, our maximum hourly wage sum cannot be higher than our highest wage ($50) * our
longest shift time (24 hours) * the number of completed timesheets. We can codify these two boundaries and see if our
program's output falls inside that for only hourly employees. We can find similar constraints for the sum of the hours
worked. Our test will look something like the following:

```js
property('handles hourly employees')
    .withGens(reportInputDataGenerator(hourlyEmployeeGenerator))
    .test(data => {
        const numTimesheets = data.timesheets.filter(ts => ts.endEpoch).length
        const summary = makeReport(data)
      
        // Check that our wages are in the correct range
        expect(summary.totals.wagesDue).toBeGreaterThanOrEqual(10 * numTimesheets)
        expect(summary.totals.wagesDue).toBeLessThanOrEqual(50 * numTimesheets * 24)

        // Check that our time worked are in the correct range
        expect(summary.totals.timeWorked).toBeGreaterThanOrEqual(numTimesheets)
        expect(summary.totals.timeWorked).toBeLessThanOrEqual(numTimesheets * 24)
    })
```

There's not a lot of code in the test itself. That's one of the nice things about property tests. The generators are
where most of the code lives, but they can be reused throughout our tests. The actual test cases themselves don't take
a lot of code to setup. And they hit our system with a lot of edge cases. If you were to look at every generated input,
you'd see that we're checking the following:
* Employees who don't have any completed timesheets
* Employees who have overlapping timesheets
* Employees who have multiple uncompleted timesheets
* Employees who have perfectly normal and valid completed timesheets

Most of those cases are "invalid" use case that "should never happen." But we are still running them through our code,
and they are still being tested, and the code still operates within the bounds we set.

> For those who don't like the fact we're testing cases that "should never happen," you can always tune
> the `employeeTimesheetGenerator` and `reportInputDataGenerator` to force only "valid" use cases. You
> can also create other data generators which filter out employees that don't have timesheets or employees that
> do have timesheets, that way you can make tests which target specific behaviors. Property testing isn't
> limited to the generators you have, you can create new generators to test either narrower or broader use cases.

### Finding Bugs with Property Tests

Next we need to handle the salaried employees. For this test, we aren't going to worry about the code or the bug report,
we'll just think of what should happen and write a property test for that behavior.

Salaried employees are paid a fixed amount per week, regardless of how much time they work. Additionally, salaried
employees probably aren't always tracking time, but they should still get paid even if they have no completed
timesheets. If there are timesheets, we should still track them in the time worked, but it shouldn't impact their wage.

Furthermore, we can easily calculate the total wage amount. Since it's based on the employee's yearly salary, it should
be their yearly salary divided by 52 (we're ignoring the cases where there are 53 weeks in a year). Our test will look
like the following:

```js
property('handles salaried employees')
    .withGens(reportInputDataGenerator(salaryEmployeeGenerator))
    .test(data => {
        // Calculate the number of completed timesheets
      // we have (used for time worked boundaries)
        const numTimesheets = data.timesheets.filter(
            ts => ts.endEpoch
        ).length
      
        // Calculate expected wages
        const expectedWages = data.employees
            .map(e => e.yearlySalary / 52)
            .reduce((a, c) => a + c, 0)
      
        // Get our report
        const summary = makeReport(data)
      
        // Check that our wages are right
        expect(summary.totals.wagesDue).toBeCloseTo(expectedWages)
      
        // "Hours worked" adjustment should match hourly adjustment
        expect(summary.totals.timeWorked).toBeGreaterThanOrEqual(numTimesheets)
        expect(summary.totals.timeWorked).toBeLessThanOrEqual(numTimesheets * 24)
    })
```

If we run this, we get some failures. Here's an example failure I got:

```text
Original Input: {
  "employees": [
    {
      "id": "39776001960736042490",
      "name": "J'Nancy อรรถ",
      "yearlySalary": 82033.95895358035
    }
  ],
  "timesheets": [
    {
      "employeeId": "39776001960736042490",
      "id": "937",
      "startEpoch": 1674393742,
      "endEpoch": null
    },
    {
      "employeeId": "39776001960736042490",
      "id": "1265793",
      "startEpoch": 1674689421,
      "endEpoch": null
    }
  ]
}

Expected: 1577.576133722699
Received: 0
```

What's going on here? Well, the answer is we're only calculating the salaried employee's pay if they have a valid
timesheet that week. However, in the above failure output, none of the timesheets are valid.

That is far from the only failing input data. I reran the tests and got a completely different failure for a 
different reason. Here's that failure case:

```text
[
  {
    "employees": [
      {
        "id": "71140609113360748691",
        "name": "Асель Baked Breads, LLC.",
        "yearlySalary": 95573.22763967636
      },
      {
        "id": "2516460687923025840",
        "name": "Jean Pierre Irene",
        "yearlySalary": 60240.18220376228
      }
    ],
    "timesheets": [
      {
        "employeeId": "71140609113360748691",
        "id": "743835095",
        "startEpoch": 1674322410,
        "endEpoch": 1688722410
      },
      {
        "employeeId": "71140609113360748691",
        "id": "410680",
        "startEpoch": 1674312301,
        "endEpoch": 1739112301
      },
      {
        "employeeId": "71140609113360748691",
        "id": "3898080870930852",
        "startEpoch": 1674602680,
        "endEpoch": 1696202680
      }
    ]
  }
]
Expected: 2996.4117277584355
Received: 5513.840056135175
```

In this instance, one of the salaried employees has two time sheets, so we're double counting their wage.

The problem we're having is that we coupled the salaried employee wage calculations with the timesheets calculation. But
they really should be separate. We didn't catch this earlier since we only had the test case where a salaried employee
had a single, valid timesheet. But with our property tests, we're getting both the use cases where there are no valid
timesheets, and we're getting the use cases where there are multiple valid timesheets.

Fortunately, the fix is the same for both error cases. We can fix the code by only calculating hourly wages in our
timesheet loop, and then doing a separate loop where we calculate our salaried wages. The code would look something
like the following:

```js
function makeReport(reportData) {
    const {employees, timesheets} = reportData
    const employeeStats = {}
    const totalStats = {
        timeWorked: 0,
        wagesDue: 0,
    }

    for (const timesheet of timesheets) {
        if (!timesheet.endEpoch) {
            continue
        }
        const employeeId = timesheet.employeeId
        const employee = employees.find(
            employee => employee.id === employeeId
        )

        if (!employee) {
            continue
        }

        employeeStats[employee.id] = employeeStats[employee.id]|| {
            name: employee.name,
            id: employee.id,
            timeWorked: 0,
            wagesDue: 0,
        }

        const start = new Date(timesheet.startEpoch)
        const end = new Date(timesheet.endEpoch)
        const hours = dateFns.differenceInHours(end, start)
        employeeStats[employee.id].timeWorked += hours
        totalStats.timeWorked += hours

        // We calculate salaried wages separately
        if (employee.yearlySalary) {
            continue
        }

        // Calculate hourly wages
        const wagesDue = hours * employee.hourlyWage;
        employeeStats[employee.id].wagesDue += wagesDue
        totalStats.wagesDue += wagesDue
    }

    // Calculate salaried wages
    for (const employee of employees) {
        if (!employee.yearlySalary) {
            continue
        }
        employeeStats[employee.id] = employeeStats[employee.id]|| {
            name: employee.name,
            id: employee.id,
            timeWorked: 0,
            wagesDue: 0,
        }
        const wagesDue = employee.yearlySalary / 52
        employeeStats[employee.id] = wagesDue
        totalStats.wagesDue += wagesDue
    }

    return {
        totals: totalStats,
        byEmployee: Object.values(employeeStats)
    }
}
```

If we rerun our tests, we'll see that they all pass! We've now added our test cases, fixed our class, and resolved the bugs.

> If you want to check more behaviors, go ahead. There are a few more things to check that we're summing properly.
> * Check the commutative/associative properties. This can be done by shuffling the timesheets/employees and making sure you get the same output
> * Check the identity properties. This can be done by adding new incomplete timesheets and making sure that the values don't change
> * Check what happens when there are timesheets without any time. This does change our boundaries, so update those as needed
> * Fix the performance issues with 1000+ arrays of data

## Short-comings of Property Tests

Property tests aren't all sunshine and roses. There are some significant shortcomings to property tests that need to be
addressed. Some of these have to deal with perception. Others are actual pain points.

### Appearing Correct vs Being Correct

One of the biggest short-comings of property tests is appearance. Example test suites appear to be more correct than
property test suites. Let me illustrate this with a point. Which of the following test suites appears to be more correct
at first glance?

```js
function fizzBuzz(number) {
    // code goes here
}

describe('test by example', () => {
    it('Handles numbers not divisible by 3 or 5', () => {
      expect(fizzBuzz(1)).toEqual('1')
      expect(fizzBuzz(2)).toEqual('2')
      expect(fizzBuzz(4)).toEqual('4')
    })
  
    it('Handles numbers divisble by 3', () => {
      expect(fizzBuzz(3)).toEqual('Fizz')
      expect(fizzBuzz(6)).toEqual('Fizz')
      expect(fizzBuzz(9)).toEqual('Fizz')
      expect(fizzBuzz(12)).toEqual('Fizz')
    })
  
    it('Handles numbers divisible by 5', () => {
      expect(fizzBuzz(5)).toEqual('Buzz')
      expect(fizzBuzz(10)).toEqual('Buzz')
      expect(fizzBuzz(20)).toEqual('Buzz')
      expect(fizzBuzz(25)).toEqual('Buzz')
    })
  
    it('Handles numbers divisible by 3 and 5', () => {
      expect(fizzBuzz(15)).toEqual('FizzBuzz')
      expect(fizzBuzz(30)).toEqual('FizzBuzz')
      expect(fizzBuzz(60)).toEqual('FizzBuzz')
    })
})

describe('property test by example', () => [
    it ('does fizzBuzz', () => {
        property('numbers not divisible by 3 or 5')
            .withGens(
                filter(
                    ints({min: 0}),
                    value => value % 3 !== 0 && value % 5 !== 0
                )
            )
            .test((num) => {
                expect(fizzBuzz(num)).toEqual(`${num}`)
            })

      property('numbers divisible by 3')
              .withGens(
                  filter(
                      ints({min: 0}),
                      value => value % 3 === 0
                  )
              )
              .test((num) => {
                expect(fizzBuzz(num)).toMatch(/^Fizz/)
                if (num % 5 !== 0) {
                  expect(fizzBuzz(num)).toEqual("Fizz")
                }
              })

      property('numbers divisible by 5')
              .withGens(
                  filter(
                      ints({min: 0}),
                      value => value % 5 === 0
                  )
              )
              .test((num) => {
                expect(fizzBuzz(num)).toMatch(/Buzz$/)
                if (num % 3 !== 0) {
                  expect(fizzBuzz(num)).toEqual("Buzz")
                }
              })
    })
])
```

A valid FizzBuzz will pass both tests. The example based testing is very natural to developers. It shows the inputs, it
shows the outputs. Developers can think through the inputs. It's easy to put a breakpoint on a failing example and to
start walking through the code. And, it gets 100% line coverage for most FizzBuzz implementations out there. It's super
nice and cozy.

The property tests looks unnatural. We're generating values, filtering out some, and then expecting them to match either a
specific string or a regex (regexes are pretty scary - so this just makes it worse). We don't even have a test case which
shows that the `fizzBuzz` method could output `FizzBuzz`! Not to mention there isn't a clear way to debug it, at least
not without creating a whole new test case to put in the "original input" into, and then set a breakpoint. Plus, devs
can't see the actual values going in or out, so it's harder to reason about if the tests are correct. It's not cozy, and
it doesn't look right.

And yet, our property test code will catch more incorrect FizzBuzz implementations than our example-based test code.
Consider the following monstrosity:

```js
function fizzBuzz(number) {
    if (number === 1 || number === 2 || number === 4) {
        return '' + number
    }
    if (number === 3 || number === 6 || number === 9 || number === 12) {
        return 'Fizz'
    }
    if (number === 5 || number === 10 || number === 20 || number === 25) {
        return 'Buzz'
    }
    return 'FizzBuzz'
}
```

Or consider this implementation:

```js
// Does fizzbuzz for numbers 1 - 100
function fizzBuzz(number) {
    if (number > 100) {
        throw new Error('Can only support up to 100')
    }
    let res = ''
    if (number % 3 === 0) {
        res += 'Fizz'
    }
    if (number % 5 === 0) {
        res += 'Buzz'
    }
    return res || '' + number
}
```

Both of those implementations pass our example test suite, but neither of them pass our property test suite. And yet,
many developers would still choose the example test suite over the property test suite. Why? Because of appearances.

When we see "input goes in, output comes out, do check", we quickly grasp that concept. We can think about
the input and reason about the output. We could sit down with a pencil and paper and manually walk through the code to
get the same result. But property tests don't work like that. Not only do they not work like that, but they can often
include a vagueness that's uncomfortable.

Remember our boundaries from earlier? We never checked the exact value of what the hourly wages should be. We only
checked that it was in a (rather large) range of values. That type of check is unsettling. What if there's an off-by-one
error? What if someone added a small addition that didn't get accounted for, would we ever know? The vagueness that is
inherit to property test is always going to be a barrier when introducing them to developers who are used to the
explicit nature of example based testing.

### Randomness and Flakiness

Property tests are designed to use randomness for testing. Every run of a property test cannot check every possible
value. Instead, they only sample a random subset of values, and that random subset changes on every execution. By
changing the sampled values, property tests can test a much wider range of values across multiple runs.

However, at the same time, developers need to have some form of consistency. Developers don't want their critical
production fix blocked because a property test finally picked a seed that finds a 20-year old bug that can only happen
0.00000000000000000000001% of the time.

Fortunately, most frameworks (including mine) allow developers to set the random seed through an environment variable.
This allows developers to run many different sample sets locally, while still having consistent behavior in the global
build and deploy infrastructure.

However, the randomness can still be problematic. Some sporadic test failures locally can cause frustration and lead
many devs down rabbit holes trying to hunt down and fix the same issue. Meanwhile, the tests could pass locally but
still fail on the build server due to mismatched seeds. This means that locking down the seed can't be the only solution.

We can also need to make sure we have proper boundaries and generators setup. Adding limits to what a generator
can produce greatly helps. Instead of testing that the output of a function works for all possible values of a 64-bit
int, just check that it works for a known safe subset of values (one where you don't get integer overflow).

Also, instead of creating property tests that work for all possible account types, break the problem down into smaller subsets
of account types and check the properties individually (similar to how we tested hourly and salaried separately).

These techniques decrease the amount of sample space left "unchecked" in each test run. This also means that the range
of inputs doesn't change drastically from test run to test run which helps to lower flakiness.

### Regression Testing

Where example tests excel at and where property tests struggle is with regression testing. Often, regression tests arise
when there was a bug that got fixed and a test was added to prevent the bug from happening again. When this happens,
the developer generally has a specific set of inputs and expected outputs already in hand, usually obtained from the
debugging process. It's a lot easier to just add an example based test using those inputs, rather than try to figure
out what the behavior and generators should do.

Additionally, property tests tend to be more lax in what they accept as valid. Consequently, it may be harder to tune
a property test to the point that a developer is confident it won't miss a very specific regression.

> If a property test is properly tuned, it can check for a whole set of regressions. But that is generally
> more complicated than a simple "let's just add a test with the data we know previously failed."

Additionally, a lot of bug fixes tend to be high priority, meaning there is someone waiting for it to get fixed so that
they can do their job. Often, this leads to pressure to quickly add any test in as a regression test, even if it is
suboptimal.

I understand why many developers would choose to forgo property tests and add in an example test instead. Because of
this, I personally believe that the goal of every property test framework should be to integrate with existing test
frameworks and work alongside integration tests. A healthy testing code base will use a combination of property tests
and example tests.

### Bias

Remember how I said bias was one of the major drawbacks of example based testing? Well, it exists in property tests too.
There is bias in which properties are tested, in what limitations are added to the code, in what constraints are placed
on the generators. Earlier in this post I had a bias where the end time would always come after the start time. However,
that's not always the case. If a user hits a clock in button and then quickly hits the clock out button, lots of weird
issues can happen.

For instance, servers do not have system clocks which are perfectly in sync. This means that really small errors can occur
where two servers report times that are slightly off. So, if the user clocks in and clocks out really fast, the clock in
request could go to Server A which reports a clock-in time of `1674172801` and Server B could report a clock-out time of
`1674172800`. To our system, this would look like the user clocked in, then went back in time and clocked out a second
earlie, which would create a timesheet of -1 seconds.

> The "hit one button then the other" scenario is actually extremely common. Often, designers have buttons "transform" into another
> button when pressed. It's not uncommon for a "clock in" button to transform into a "clock out" button. Which means that
> when a user double taps or double-clicks the "clock in" button (usually on accident), then they would really clock in
> and then immediately clock out.

Because I didn't check for that scenario, there is a bug where users could lose time due to accidentally double tapping
a button on their phone. This is an example of a bias.

That said, property tests do help limit the amount of biases which end up in the tests. They brute force hundreds to
thousands of examples rather than half a dozen, and they do create inputs that a developer probably didn't anticipate.
This allows for finding additional bugs, but it isn't a silver-bullet which will find all of your bugs.

## Conclusion

Property testing is very powerful as it is basically a specialized fuzzer for a codebase. The more chaotic, brute force nature of
property tests allows it to find bugs the developers may not have anticipated, even when the developer had 100% coverage
with traditional tests.

However, property tests aren't without their drawbacks either. They are harder to understand, and it's harder to for
developers to know if the test is correct or not. They introduce a risk of flakiness which can be undesirable for
many processes without proper mitigations. And they take quite a bit more thought and effort to write, especially when
there is an existing set of input and output data. Plus, they don't remove all biases, so even a rigorous property test
suite can still miss things.

Property tests can (and should) be used alongside example tests. Example test are great for getting tests added quickly,
especially when the example was obtained previously (e.g. through debugging, through a specification, etc). Property
tests can then add an extra layer of checks on-top to see if something might have been overlooked.

### List of frameworks

If you'd like to start using property test, here's a list of libraries and frameworks I found for different languages.
I haven't used most of them, but the ones I have used (or written) are annotated.

#### C++

* (C++ Property Testing)[https://gitlab.com/mtolman/c-property-testing]
  * My framework
* (Rapidcheck)[https://github.com/emil-e/rapidcheck]
  * This was the framework I used before making my own. Really well-made, I still highly recommend it
* (CppQuickCheck)[https://github.com/grogers0/CppQuickCheck]

#### Clojure

* (clojure.spec)[https://clojure.org/about/spec]
  * Also really good. This was the first property testing framework I ever used. It also integrated into documentation generation


#### Go

* (rapid)[https://github.com/flyingmutant/rapid]
* (gopter)[https://pkg.go.dev/github.com/leanovate/gopter]

#### Haskell

* (QuickCheck)[https://hackage.haskell.org/package/QuickCheck]
  * The basis for almost all property test frameworks

#### Java

* (jqwik)[https://jqwik.net/]

#### JavaScript

* (@matttolman/prop-test)[https://gitlab.com/mtolman/js-prop-tests/]
  * My framework
* (JsVerify)[https://jsverify.github.io/]
* (fast-check)[https://github.com/dubzzz/fast-check]

#### .NET

* (FsCheck)[https://fscheck.github.io/FsCheck/]

#### Python

* (hpothesis)[https://github.com/HypothesisWorks/hypothesis]

#### Rust

* (quickheck)[https://crates.io/crates/quickcheck]
* (proptest)[https://crates.io/crates/proptest]

@References

* smtp-ipv6
  | type: web
  | link: https://www.rfc-editor.org/rfc/rfc5321#section-4.1.3
  | page-title: Simple Mail Transfer Protocol
  | website-title: RFC Editor
  | date-accessed: 2022-10-23
* addition-properties
  | type: web
  | link: https://en.wikipedia.org/wiki/Addition#Properties
  | page-title: Addition
  | website-title: Wikipedia
  | date-accessed: 2022-10-23
* floating-point-errors
  | type: web
  | page-title: What is a floating point number, and why do they suck
  | website-title: Intuit Blog
  | link: https://www.intuit.com/blog/author/ted-drake/#:~:text=%EE%80%80Ted%20Drake%EE%80%81%20%EE%80%80Ted%20Drake%EE%80%81%20is%20an%20experienced%20engineer%2C,employees%20and%20promotes%20Intuit%E2%80%99s%20diversity%20in%20hiring%20programs.
  | date-accessed: 2022-10-20

