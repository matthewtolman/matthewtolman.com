# What is property testing?

Property testing is a developmental approach to writing tests (both unit tests and integration tests) which
helps developers find the limitations of their software. It isn't an all-encompassing set of principles
or practices which impact the development lifecycle, unlike Test Driven Development.

Instead, property tests focus on giving developers more tools for the actual test implementation, regardless
of the testing practices which exist. Additionally, these tools aim to provide far better coverage and depth
to testing than simple "line coverage" and "branch coverage" metrics can provide.

This is done by introducing a small section of code which generates relatively large amounts of randomized data which
is then fed into the program (or parts of the program), and then monitoring the behavior of the program to see if the
program operates as expected. Many property testing frameworks will introduce inputs that strain the boundaries which
developers don't always think about, such as number overflows and rounding errors. Other frameworks take it a step
further and can check for additional errors, such as byte order mismatches, invalid handling of obscure portions of
a standard (e.g. IPv6 hosts for email addresses), and more. Many frameworks also provide developers the tooling they
need to generate their own checkers which can check for their specific domain's edge cases.

This article aims to be an introduction to property testing. I will also introduce a property testing framework which
I have written in [C++](https://gitlab.com/mtolman/c-property-testing) and [JavaScript/TypeScript](https://gitlab.com/mtolman/js-prop-tests/).
In a future article I will discuss how I made the frameworks, some of the design decisions (and mistakes) I made, and
what the future holds for those property testing frameworks (including potential ports to other languages).

But for now, let's take a look at how most developers typically write their tests, examine the pitfalls commonly
experienced, and see how property testing can help. I'm going to use a fairly complex example (at least more complex
than you traditionally get in a blog post) to demonstrate the rationale, mostly since the other blog posts I've seen on
property testing use examples so trivial they don't properly demonstrate the utility of property testing.

[toc]

## The Example Problem

For this example, we are working on time tracking software. Our clients are businesses, and they use our software to track
how long each of their employees work, that way they can pay their employees hourly. Our clients (the businesses) would
like to have some reports where for any given week they could see the total time worked by all employees, the amount of
money they have to pay employees, and see a breakout by employee.

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
      "userId": "123942"
    },
    {
      "id": "4958034",
      "startEpoch": 1674223351000,
      "endEpoch": null,
      "userId": "9857847"
    }
  ]
}
```

In the above data, you'll notice that we have both salaried and hourly workers. Hourly workers have an hourly rate
(we're ignoring overtime rules for this example). Meanwhile, salaried workers have just their yearly salary.

You'll also notice that there are some timesheets without an ending epoch. These are for timesheets where the employee
is still clocked in. For our report, we'll just filter out those timesheets. As for the start and end times, we're using
the UTC epochs in milliseconds (since JavaScript's `Date` class works in milliseconds). We're also just using normal
numbers for the wages and hours. This is a blog post example, so we're taking the shortcut of using a double even though
there can be weird rounding bugs (e.g. `0.1 + 0.2`). Our IDs are also all strings, even though they are numeric. There
are many reasons for this (e.g. wanting to use 64-bit integer ids, planning a migration to a UUID, etc.). For our example,
the reason is that we're using JSON and JSON prefers string ids.

We'll also assume that we're only ever querying by a week at a time. We'll also assume that there are always 52 weeks in
a year. Reality is a lot more complicated than that, but the focus of this example is on showing testing patterns and
not making production ready software.

For this article, we'll be writing our code in JavaScript (using Node.js to run it), and we'll be using the testing
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

The code for the example isn't too complex, mostly since we've isolated the logic from any database calls. First, we need
to extract our user and timesheet data, as well as initialize our data accumulators. I'm putting the code in a file
called `code.js` (only really relevant once we start importing the code into our test files).

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

Next we need to iterate over all of our timesheets and filter out anything which we're going to ignore.

```js
    for (const timesheet of timesheets) {
        if (!timesheet.endEpoch) {
            continue
        }
```

We then need to find the employee tied to the timesheet. We'll also pull in their accumulated data (or initialize it if
it doesn't exist).

```js
        const userId = timesheet.userId
        const employee = employees.find(employee => employee.id === userId)

        employeeStats[employee.id] = employeeStats[employee.id] || {
            name: employee.name,
            id: employee.id,
            timeWorked: 0,
            wagesDue: 0,
        }
```

Next we need to get the number of hours worked. Since we've already filtered out any dates with a null end date, we can
just grab our end date epoch. We'll use DateFns to calculate the number of hours worked. We can also add the hours worked
to our stats.

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


        const wagesDue = (employee.yearlySalary) ? employee.yearlySalary / 52 : hours * employee.hourlyWage;
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

Here's the full method:

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
        const userId = timesheet.userId
        const employee = employees.find(employee => employee.id === userId)

        employeeStats[employee.id] = employeeStats[employee.id] || {
            timeWorked: 0,
            wagesDue: 0,
        }

        const start = new Date(timesheet.startEpoch)
        const end = new Date(timesheet.endEpoch)
        const hours = dateFns.differenceInHours(end, start)
        employeeStats[employee.id].timeWorked += hours
        totalStats.timeWorked += hours

        const wagesDue = (employee.yearlySalary) ? employee.yearlySalary / 52 : hours * employee.hourlyWage;
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
doing TDD (maybe sometime I'll write a post sharing my thoughts on TDD). Instead, we wrote the code once, and then we
wrote the tests. This is generally the process that I find most developers follow at most places, and it does well enough.

In our hypothetical example, our company set an enforced goal of 80% line and branch coverage. Without meeting that
coverage our code cannot be deployed. So, we have to write enough tests to hit that coverage. This is a fairly common
practice though the number varies a lot (I've seen as low as 80% and as high as 98%).

With that in mind, let us begin our testing.

The first test we'll write is simple. Without any data, we should get back zeroes. Let's write that test:

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

Already we're at 33% function coverage. Now we just need to get the rest of the way there. We can do that by adding a
few more tests. One for handling hourly wages, and one for handling salaried wages. Part of our specification included
some example data for hourly and salaried employees, so we'll put that data into our tests.

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
                    userId: "123942"
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
                    "id": "4958034",
                    // This is a time period of 12 hours
                    startEpoch: 1674223351000,
                    endEpoch: 1674266551000,
                    "userId": "9857847"
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

Now let's recollect our code coverage by running `npx jest --collectCoverage` again. Here's the output:

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

There's only one line not covered, and that's the filter for when we don't have an end epoch. We could add in that test
and get our coveted 100% coverage, but we also hit our 80% bar. This is where many devs would stop testing. They hit the
limit, it's good to go. BUt, we're going to be extra rigorous with our tests and add that one last little test.

```js
    it('handles timesheets without ending epochs', () => {
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
                    "id": "4958034",
                    // This is a time period of 12 hours
                    startEpoch: 1674223351000,
                    endEpoch: null,
                    "userId": "9857847"
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

We now have 100% test coverage! We've done it, we've tested all of our code! We have gotten 100% test coverage, that's
the highest score there is. We can't improve it. Our coworkers congratulate us for such a good job with thorough testing.

We ship our code out to production. Everything goes well, and we move onto other features.

### False assurance

After a while, two bug reports come in for our timesheet report. Here's the first one:

> **Timesheet Report Fails**
> 
> Business Account: 5932984934
> Environment: Production
> Report Dates: 10/22/2023 - 10/29/2023
> Transaction Id: 493283029138293293282
> Log Error Message: TypeError: Cannot read properties of undefined (reading 'id')
> Description:
> 
> An employee had resigned. Before then reports worked fine. Now they don't work anymore. Please advise.

 Here's the second one:

> **Timesheet Report Wrong for Some Employees**
> 
> Business Account: 46959348204
> Environment: Production
> Report Dates: 10/22/2023 - 10/29/2023
> Transaction Id: 349593429394084832
> Log Error Message: None
> Description:
> 
> User Rob Martian is getting inaccurate wage numbers in the report. Please investigate.

Before we investigate what these errors are, let's first think about why we got them. We had 100% test coverage. We
covered every line of code. We hit every branch. Maybe we just had a requirement mismatch for the second test case, but
why are we getting a report failure? What could possibly be causing issues?

Well, the answer is that showing code is correct through tests is not about coverage. It's not about writing tests before
or after writing the actual code. It's not about having a massive test suite that's several magnitudes bigger than the
code being tested. In fact, in many ways, it's not about having examples in the code at all.

Tests that rely on examples never prove that code is correct. It only shows that it handles cases the developer both
thought of, and which the developer thought would be easy enough to write tests for. In reality, it's even worse than
that. Automated tests are the developer picking examples which confirms their own bias: that the code which they wrote
is correct and meets the specification outlined. Any business which relies on a single developer's word that the code
which that developer wrote must be correct because "coverage number high" is doomed to have a large quantity of broken
features and unplanned outages.

Code reviews can help, but code reviews are done by people who are highly incentivized to make sure that the code gets
shipped (e.g. their work depends on the code being reviewed, they're on the same team, etc.) while also being
incentivized to not spend too much time on the review (e.g. they have their own work to do). QA is great for vetting
code, but most companies are moving away from QA departments (for some reason). I've heard many reasons (fast iteration,
high cost, little benefit), but most of those reasons are either a reflection of the company's own failings (e.g. lack
of understanding on how to interact with QA) or just simply don't hold up to any scrutiny, regardless of the company
which makes them.

But that doesn't mean hope is lost. There is a third reviewer available to help, the computer (and no, I do not mean
Chat-GPT).

## Property Based Testing

Computers aren't very smart. They don't know how to accurately read code, understand what it does, and then write tests
that show the limitations of that code. However, they are really good at one thing: they can brute force a lot of
computations.

There are pieces of software which are incredibly powerful at brute forcing bugs out of almost any program, such as
[AFL](https://lcamtuf.coredump.cx/afl/), (and it's [many forks](https://github.com/Microsvuln/Awesome-AFL)). These
programs do a type of testing called [fuzzing](https://en.wikipedia.org/wiki/Fuzzing), which is incredibly powerful.
However, it also requires extra work to set up, doesn't integrate with existing test suites, and doesn't meet any
"coverage" metrics. And many companies would qualify it as a "DevOps/Ops" responsibility and not a "Developer"
responsibility (oh the DevOps irony), so it would probably not get implemented.

What we need instead is a way to brute force bugs out of our program, but to do it within our own test suites. This is
where Property Based Testing comes in.

Property Based Testing works by telling the computer "here's a program, here's how to create input for that program,
and here's how the program should behave. Now try to break it." If the computer can't break it, the test passes.
If the program does break it, the test fails.

The common introductory example I've seen is for addition. Correct addition of any two numbers should have the following
properties:
* Commutative
  * The order of addition shouldn't matter
  * a + b = b + a
* Associative
  * The pairing of multiple additions shouldn't matter
  * (a + b) + c = a + (b + c)
* Identity
  * Any number plus 0 should equal itself
  * a + 0 = a

Using [@matttolman/prop-test](https://www.npmjs.com/package/@matttolman/prop-test) we can codify this as follows:

```js
const {property, generators} = require('@matttolman/prop-test')

describe('JavaScript', () => {
    it('can add numbers', () => {
        const intGen = generators.number.ints({min: -1000, max: 1000})
        property('commutative', intGen, intGen)
            .test((a, b) => {
                expect(a + b).toEqual(b + a)
            })

        property('associative', intGen, intGen, intGen)
            .test((a, b, c) => {
                expect((a + b) + c).toEqual(a + (b + c))
            })

        property('identity', intGen)
            .test((a) => {
                expect(a + 0).toEqual(a)
            })
    })
})
```

> **Note:** In the above I'm limiting to integers in a specific range. The main reason for this two-fold. The first is
> that JavaScript uses 64-bit floating point numbers, and floating point numbers have issues^[floating-point-errors] where
> they can violate the mathematical principles above, so we need to use ints. The second reason is that computers are finite,
> which means they can only represent a finite subset of integers and if we get too close to the edge we break our mathematical
> principles again, so we have to have a limit and I chose a small limit since I didn't want to type too much.

You can take the code above and run it, and it will all pass. If you start changing the line `expect((a + b) + c).toEqual(a + (b + c))`
to `expect((a + b) + c).toEqual(a + (b - c))`, then you'll end up with  an error like the following:

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

You'll notice a few things going on with the error. The first is that while the error is a Jest error, there is a bunch
of extra stuff added in. That extra stuff is my property test framework at work. It's telling us which property failed,
the inputs which made it fail, and a "simplified" input which also fails (the simplified input is usually something
like smaller arrays, smaller numbers, matching numbers, etc.). We also get a seed which is the seed used for creating
the random values. This seed can be set in the environment variable `PROP_TEST_SEED` to replay the failing test case.

These outputted values (the inputs and the seed) allow us to start debugging our code to see what went wrong and why.
They also allow us to create a new test case with that failed example to prevent regressions. For instance, we could
add the following test case to prevent a regression with the original inputs:

```js
    it('passes regression test 1', () => {
        const a = -503
        const b = -128
        const c = -193
        expect((a + b) + c).toEqual(a + (b + c))
    })
```

Now we'll take a look at how to write tests to better check our example code.

### Improving the testing of our example code

As a reminder, here's our example code:

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
        const userId = timesheet.userId
        const employee = employees.find(employee => employee.id === userId)

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

        const wagesDue = (employee.yearlySalary) ? employee.yearlySalary / 52 : hours * employee.hourlyWage;
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

To test that code, we need the computer to generate the following:
* Random employees
* Random timesheets

Our employees have the following fields:
* `name` - The employee's name
* `id` - The employee's id
* `hourlyWage` or `yearlySalary` - How much money the employee makes

Our timesheets have the following fields:
* `id` - Timesheet id
* `startEpoch` - Starting unix timestamp
* `endEpoch` - Ending unix timestamp
* `userId` - ID of the user

We'll start off with telling the computer how to generate a person. Normally this requires a lot of effort to create
usable generators. However, my property testing library provides a lot of fake data generators (similar to FakerJs but
with extra determinism and different classifications). We'll use these to help simplify creating our data generators.

Let's start with the employee data generation:

```js
const {combine, just, map, oneOf, DataComplexity, array} = require("@matttolman/prop-test/generator/common")
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
const employeeGenerator = oneOf(hourlyEmployeeGenerator, salaryEmployeeGenerator)
```

Next let's define our timesheet generator

```js
const timesheetGenerator = map(
        combine({
          userId: numeric({minLen: 1, maxLen: 20}),
          id: numeric({minLen: 1, maxLen: 20}),
          startEpoch: ints({min: 1674172800, max: 1674734400}),
          hours: oneOf(ints({min: 1, max: 24}), just(null)),
        }),
        (ts, seed) => {
          const hours = ts.hours
          if (hours) {
            ts.endEpoch = dateFns.addHours((new Date(ts.startEpoch)), hours).getTime()
          }
          else {
            ts.endEpoch = null
          }
          delete ts.hours
          return ts
        }
)
```

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
Timesheet { id: '1702726724650', userId: '862', startEpoch: 1674631705, endEpoch: null }
Timesheet { id: '1977398106104393', userId: '3', startEpoch: 1674483020, endEpoch: 1750083020 }
```

> You may have noticed that our names have non-ascii characters, and that they have more than just the two names most
> Americans would expect for full names (first/last). This is because, by default, @matttolman/prop-test library will
> pull in more than stereotypical American names. This includes European names with accents, names with multiple parts
> such as last names with two parts (e.g. Hernandez Gonzales), names with dashes (e.g. Filipe-Romero), names with
> apostrophes (e.g. O'Connor), names that I've seen with user-initiated hacks that I've seen in B2B
> software (e.g. Jack \[Acct#155932]) and names that don't even use the latin alphabet (e.g. 志旼). You can adjust these
> settings with the minComplexity and maxComplexity parameters.
> 
> Example: `fullName(DataComplexity.RUDIMENTARY, DataComplexity.BASIC)`.

### Our first real property test

Now that we have our data generators, it's time to put them to use. Let's start by just pushing the data through and
see what we get.

```js
describe('makeReport', () => {
    it('handles random data', () => {
        property('does not crash')
            .withGens(array(employeeGenerator), array(timesheetGenerator))
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
Seed: 3241117440
Original Input: [[{"id":"79459113644","name":"You Li","hourlyWage":21.293577740735742},...
Simplified Input: [[{"id":"79459113644","name":"You Li","hourlyWage":21.293577740735742},...
------------------------------------------------

TypeError: Cannot read properties of undefined (reading 'id')
    at id (code.js:18:32)
```

Well, that didn't take long. We managed to reproduce our failed report bug report with our very first try. But why did
it crash? Well, let's take a look at the line of code and see what's going on.

```js
        const userId = timesheet.userId
        const employee = employees.find(employee => employee.id === userId)

        employeeStats[employee.id] = employeeStats[employee.id]|| { /* truncated for brevity */ }
```

What we're doing here is we're getting the user id from the timesheet and seeing if there's an employee with that id.
We then grab the "id" property off of our employee. This code here assumes that we will always find an employee. But
it's failing. Why? well, if we take a look at our data we'll notice that there's no connection between the employee
ids and the timesheet ids. Every timesheet is tied to an employee which doesn't exist!

But, why did we get that error in production? Well if we look at the description again, it says "An employee had
resigned. Before then reports worked fine. Now they don't work anymore." In other words, an employee had resigned,
their record was deleted, and now when the business runs reports they're getting timesheets that are tied to a user
which no longer exists.

> Here "deleted" could be a soft delete or a hard delete. Often queries filter out any employee/user records which
> are soft deleted, so it's quite possible this same bug would appear in a soft-delete system, even though the record
> does exist in the database! 

There are a few potential solutions here: filter out timesheets with missing users, or tie the timesheets to an "unknown"
user, or only include the stats in the totals and don't provide an individual breakout. For this blog post, we'll just
filter out the users. The following code should do that:

```js
        const userId = timesheet.userId
        const employee = employees.find(employee => employee.id === userId)

        if (!employee) {
            continue
        }
        // rest of code goes here
```

Now if we rerun our tests, we'll see that they just hang. Well, they aren't really hanging, they're just taking a really
long time. That's because the default maximum size for generated arrays is 1024 elements. Obviously we have some sort of
performance issue, but right now we're trying to get our code correct before we make it fast. For now, let's make the
maximum array size a lot smaller.

```js
property('does not crash with possibly missing ids')
    .withGens(array(employeeGenerator, {maxLen: 10}), array(timesheetGenerator, {maxLen: 10}))
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

The code is reporting full coverage, but how can that be? Well, a lot of it has to do with how I wrote the built in
generators. The generators aren't designed to have high entropy or produce truly "random" results. They're designed to
be extremely deterministic, almost to a fault. This means that, by chance, the computer can "accidentally" create an
employee and timesheet with correlated ids. And, because of how I wrote the code, that chance is pretty high.

> **Note:** depending on when you read this, I may have increased the entropy of the generators. If so, then the above
> statement may no longer be accurate and you may be getting much less test coverage.

However, relying on me not changing the amount of entropy in my library isn't a good habit to develop. For one, I'm just a
random developer on the web making an open/public source project that I can change at any point in time. I can always
change my generation algorithms to have a higher amount of entropy, which could break your code and cause lots of
headaches for you (but not me). Second, most other property testing frameworks don't have entropy as low as my
framework. So, if you ever decide to use another system (or have to use another system), then you'll need to know
how to adapt. Fortunately, it's pretty easy to adapt by creating generators which add a correlation relationship
between the outputs of our generators.

### Correlating Records

Correlation generators generate data from other generators. They then go through and change the output data to match
the system's constraints (e.g. change ids to form a relationship). Below is an example which correlates employees and
timesheets:

```js
// Create an employee and a bunch of timesheets tied to that employee
const employeeTimesheetGenerator = (makeEmployees = employeeGenerator) => map(
        // First we create our employee and timesheets
        combine({
          employee: makeEmployees,
          timesheets: array(timesheetGenerator, {maxLen: 5})
        }),
        ({employee, timesheets}) => {
            // next, we iterate over every timesheet and update the user id
            // to match our employee's id, that way it's tied to our employee 
            for (let i = 0; i < timesheets.length; ++i) {
                timesheets[i].userId = employee.id
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
crashes or not). For this, we need to come up with some other behavior we're looking for. For these behaviors, we're
defining boundaries that the program operates in rather than defining actual values. For instance, instead of checking
for the exact value of hours worked, we instead define boundaries for the upper and lower bounds of how many hours could
possibly be worked, and then we check that the program's output falls within those boundaries.

For wages of hourly employees, we're looking for sum of the wages to fall within a specific boundary. On the low end,
we have a minimum wage ($10) in our data, and a minimum number of hours for completed timesheets (1 hour), and a number
of completed timesheets. Our total hourly paid wage cannot go below $10 * 1 hour * number of completed timesheets. On
the other end, our maximum hourly wage sum cannot be higher than our highest wage ($50) * our longest shift time
(24 hours) * the number of completed timesheets. We can codify these two boundaries and see if our program's output
falls inside that for only hourly employees. We can find similar constraints for the sum of the hours worked.
Our test will look something like the following:

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

That's not a lot of code in the test itself. That's one of the nice things about property tests. They don't take a lot
of code to write. And the above test is hitting our system with a lot of edge cases. If you were to look at every output
from the 500 iterations, you'd see some where employees don't have any completed timesheets, some where employees have
overlapping timesheets, some where employees have multiple uncompleted timesheets, and some where employees have perfectly
normal and valid completed timesheets. Most of those cases are "invalid" use case that "should never happen." But we are
still running them through our code, and they are still being tested, and the code still operates within the bounds we
set.

> For those who don't like the fact we're testing cases that "should never happen," you can always tune
> the `employeeTimesheetGenerator` and `reportInputDataGenerator` to force only "valid" use cases. You
> can also create other data generators which filter out users that don't have timesheets or users that
> do have timesheets, that way you can make tests which target specific behaviors. Property testing isn't
> limited to the generators you have, you can always combine generators to make a generator that you need.

### Finding Bugs with Property Tests

Next we need to handle the salaried employees. For this test, we aren't going to worry about the code, we'll just think
of what should happen and write a property test for that behavior.

Salaried employees are paid a fixed amount per week, regardless of how much time they work. Additionally, salaried
employees probably aren't always tracking time, but they should still get paid even if they have no completed
timesheets. Though if there are timesheets, we should still track them in the time worked.

Furthermore, we can easily calculate the total wage amount. Since it's based on the employee's yearly salary, it should
be their yearly salary divided by 52 (we're ignoring the cases where there are 53 weeks in a year). Our test will look
like the following:

```js
property('handles salaried employees')
    .withGens(reportInputDataGenerator(salaryEmployeeGenerator))
    .test(data => {
        // Calculate the number of completed timesheets we have (used for time worked boundaries)
        const numTimesheets = data.timesheets.filter(ts => ts.endEpoch).length
      
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
      "userId": "39776001960736042490",
      "id": "937",
      "startEpoch": 1674393742,
      "endEpoch": null
    },
    {
      "userId": "39776001960736042490",
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
timesheet that week. However, in the above example, none of the timesheets are valid.

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
        "userId": "71140609113360748691",
        "id": "743835095",
        "startEpoch": 1674322410,
        "endEpoch": 1688722410
      },
      {
        "userId": "71140609113360748691",
        "id": "410680",
        "startEpoch": 1674312301,
        "endEpoch": 1739112301
      },
      {
        "userId": "71140609113360748691",
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
they really should be separate.

We didn't catch this earlier since we only had the test case where a salaried employee had a single, valid timesheet.
But with our property tests, we're getting both the use cases where there are no valid timesheets, and we're getting the
use cases where there are multiple valid timesheets.

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
        const userId = timesheet.userId
        const employee = employees.find(employee => employee.id === userId)

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
            .withGens(filter(ints({min: 0}), value => value % 3 !== 0 && value % 5 !== 0))
            .test((num) => {
                expect(fizzBuzz(num)).toEqual(`${num}`)
            })

      property('numbers divisible by 3')
              .withGens(filter(ints({min: 0}), value => value % 3 === 0))
              .test((num) => {
                expect(fizzBuzz(num)).toMatch(/^Fizz/)
              })

      property('numbers divisible by 5')
              .withGens(filter(ints({min: 0}), value => value % 5 === 0))
              .test((num) => {
                expect(fizzBuzz(num)).toMatch(/Buzz$/)
              })
    })
])
```

A valid FizzBuzz will pass both tests. The example based testing is very natural to developers. It shows the inputs, it
shows the outputs. Developers can think through the inputs. It's easy to put a breakpoint on a failing example and to
start walking through the code. And, it gets 100% line coverage for most FizzBuzz implementations out there. It's super
nice and cozy.

The property tests looks a unatural. We're generating values, filtering out, and then expecting them to match either a
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

When we see "input goes in, output comes out, check they're the same", we quickly grasp that concept. We can think about
the input and reason about the output. We could sit down with a pencil and paper and manually walk through the code to
get the same result. But property tests don't work like that. Not only do they not work like that, but they can often
include a vagueness.

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

Instead, we also need to make sure we have proper boundaries and generators setup. Adding limits to what a generator
can produce greatly helps. Instead of testing that the output of a function works for all possible values of a 64-bit
int, just check that it works for a known safe subset of values (one where you don't get integer overflow).

Instead of creating property tests that work for all possible account types, break the problem down into smaller subsets
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

> On the other hand, if a property test is properly tuned, it can check for a whole set of regressions. But that is generally
> more complicated than a simple "let's just add in this test data."

Additionally, a lot of bug fixes tend to be high priority, meaning there is someone waiting for it to get fixed so that
they can do their job. Often, this leads to pressure to quickly add any test in as a regression test, even if it is
suboptimal.

I understand why many developers would choose to forgo property tests and add in an example test instead. Because of
this, I personally believe that the goal of every property test framework should be to integrate with existing test
frameworks and work alongside integration tests. A healthy testing code base will use a combination of property tests
and example tests. 

## Conclusion

Property testing is very powerful as it is basically a specialized fuzzer for a codebase. It can find a lot of bugs
pretty quickly, including bugs that can be missed by 100% test coverage. The more chaotic, brute force nature of
property tests allows it to find bugs the developers may not have anticipated.

However, property tests aren't without their drawbacks either. They are harder to understand, and it's harder to for
developers to know if the test is correct or not. They introduce a risk of flakiness which can be undesirable for
many processes without proper mitigations. And they take quite a bit more thought and effort to write, especially when
there is an existing set of input and output data.

Yet property tests provide substantive benefits. They can allow developers to make thorough tests which can find hidden
issues. Additionally, property tests can (and should) be used alongside example tests. Example test are great for getting
tests added quickly, especially when the example was obtained previously (e.g. through debugging, through a specification,
etc). Property tests can then add that extra assurance on-top of the example tests to make sure that something wasn't
overlooked.

### List of frameworks

If you'd like to start using property test frameworks, here's a list for different languages:

#### C++

* [C++ Property Testing](https://gitlab.com/mtolman/c-property-testing) (my framework)
* [Rapidcheck](https://github.com/emil-e/rapidcheck)
  * This was the framework I used before making my own. Really well-made, I still highly recommend it
* [CppQuickCheck](https://github.com/grogers0/CppQuickCheck)


#### Haskell

* [QuickCheck](https://hackage.haskell.org/package/QuickCheck)
  * The basis for almost all property test frameworks

#### JavaScript

* [@matttolman/prop-test](https://gitlab.com/mtolman/js-prop-tests/) (my framework)
* [JsVerify](https://jsverify.github.io/)
* [fast-check](https://github.com/dubzzz/fast-check)

#### Java

* [jqwik](https://jqwik.net/)

#### Clojure

* [clojure.spec](https://clojure.org/about/spec)
  * Also really good. This was the first property testing framework I ever used. It also integrated into documentation generation

#### .NET

* [FsCheck](https://fscheck.github.io/FsCheck/)

#### Rust

* [quickheck](https://crates.io/crates/quickcheck)
* [proptest](https://crates.io/crates/proptest)

@References

* floating-point-errors
  | type: web
  | page-title: What is a floating point number, and why do they suck
  | website-title: Intuit Blog
  | link: https://www.intuit.com/blog/author/ted-drake/#:~:text=%EE%80%80Ted%20Drake%EE%80%81%20%EE%80%80Ted%20Drake%EE%80%81%20is%20an%20experienced%20engineer%2C,employees%20and%20promotes%20Intuit%E2%80%99s%20diversity%20in%20hiring%20programs.
  | date-accessed: 2022-10-20

