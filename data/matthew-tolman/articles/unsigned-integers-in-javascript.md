Lately I've been experimenting with creating random data generators in TypeScript. I wanted to avoid using the global random number generator `Math.random` since I wanted my generators to be fully determinitic (i.e. if you give them the same input, they will always return the same output). I also wanted a seed to be passed in every time, which meant that I'd be creating a new generator every time.

My first approach was to write a TypeScript implementation of the Mersenne Twister algorithm. Which worked, but it turns out Mersenne Twister takes a lot of memory (~2.5 KB). That means 2.5 KB of memory allocation and initialization every time my code was called. It also meant that the allocated memory lost all references and ended up waiting for the GC to run, which created quite a bit of memory pressure.

So, I started looking at alternatives and found TinyMT^[tinymt], which is a memory-friendly version of Mersenne Twister written in C. I decided to port it over to TypeScript, and I ran into a lot of issues due to the differences between C and JavaScript/TypeScript when it comes to numbers. Specifically, I had a lot of issues with unsigned integer arithmetic. After I got it all working, I decided to write this blog post about my journey with unsigned integer arithmetic in TypeScript/JavaScript.

[toc]

## Overview of TinyMT

To start off with, let's cover TinyMT. TinyMT is a C library for generating random numbers.  There are two variants: a 64-bit variant and a 32-bit variant. Since TypeScript and JavaScript don't allow 64-bit integers, we'll just focus on the 32-bit variant. Both variants don't do any dynamic memory allocation (i.e. they don't use the heap). They do this by having all of it's state inside the following small struct:

```c
// Just showing the 32-bit variant
struct TINYMT32_T {
    uint32_t status[4];
    uint32_t mat1;
    uint32_t mat2;
    uint32_t tmat;
};
```

The fields are as follows:
- **status** - the current internal state of the PRNG
- **mat1** - The first constant for initialization and state flags
- **mat2** - The second constant for initialization and state flags
- **tmat** - The third constant for initialization and flag for retrieving values

Exactly what the phrases `mat1`, `mat2`, and `tmat` stand for, I'm not sure and I couldn't find an explanation. I only have a description based on how they're used in the code. Altogether, we end up with 128 bits for the internal state, 96 bits for various flags, and 32 bits for alignment which ends up being 256 bits. That's way smaller than our 2.5KB for a full Mersenne Twister.

> One thing to keep in mind is that TinyMT has a much smaller repeat period (\(2^{127} - 1\) as opposed to Mersenne Twister's \(2^{19937} - 1\)). I'm only doing a few hundred calls at most, so I haven't ran into issues with TinyMT. But your mileage may vary.

As for the values of the constants, I'm just using values I found in the test suites. They are as follows:

| Constant | Value |
|----------|-------|
| mat1     | 0x8f7011ee |
| mat2     | 0xfc78ff1f |
| tmat     | 0x3793fdff |

TinyMt has a few core methods:

- **init** - Initializes the struct
- **next_state** - Updates the struct to the next state
- **temper** - Gets a uint32 from the current state
- **temper_conv** - Gets a float32 from the current state

The library works almost entirely with unsigned 32-bit integers. This will become problematic as we try to port over to JavaScript/TypeScript since there isn't an unsigned 32-bit integer type.

## Initial port to JavaScript

For our initial port, we're just going to translate all the code over as-is. We'll figure out the unsigned integer issues later on.

Since we have TypeScript classes, we'll turn the init function into a class constructor. We'll also use a class for the state. Here's my initial port without any integer handling (with TinyMT's C code as comments for reference):

```typescript
class RandomState {
    // uint32_t status[4];
    public status: number[] = (new Array(4)).fill(0);
    // uint32_t mat1;
    public mat1: number = 0x8f7011ee;
    // uint32_t mat2;
    public mat2: number = 0xfc78ff1f;
    // uint32_t tmat;
    public tmat: number = 0x3793fdff;
}
export default class Random {
    private state = new RandomState()

    // void tinymt32_init(tinymt32_t * random, uint32_t seed)
    public constructor(seed: number = defaultSeed()) {
        // random->status[0] = seed;
        this.state.status[0] = seed
        // random->status[1] = random->mat1;
        this.state.status[1] = this.state.mat1
        // random->status[2] = random->mat2;
        this.state.status[2] = this.state.mat2
        // random->status[3] = random->tmat;
        this.state.status[3] = this.state.tmat


        for (let i = 1; i < minLoop; ++i) {
            // Note: in the original C, this was all one statement
            // I broke it into smaller statements for readability

            // uint32_t xorL = random->status[(i - 1) & 3];
            const xorL = (this.state.status[(i - 1) & 3]);
            // uint32_t xorR = (random->status[(i - 1) & 3] >> 30);
            const xorR = this.state.status[(i - 1) & 3] >> 30;
            // uint32_t mult = UINT32_C(1812433253) * (xorL ^ xorR);
            const mult = 1812433253 * (xorL ^ xorR);
            // uint32_t sum = i + mult;
            const sum = i + mult;

            // random->status[i & 3] ^= sum;
            this.state.status[i & 3] = (this.state.status[i & 3] ^ sum);
        }

        // Debug statement
        console.log(this.state.status)
        // ... rest of code goes here
    }

    // ... Rest goes here
}
```

At this point, we already have a pretty significant divergence from the original C code. If we run the TypeScript code with the seed `2545341989`, we'll end up with the following state array:

```json
[ 1409262081, -1124371832, 867272735, -2057235969 ]
```

However, the orignal C code has the following state:

```json
[3911076234, 511429786, 2775206926, 3754336954 ]
```

What's going on here? Well, we it has to do with how JavaScript handles bitwise operators and multiplication.

## Peeking under the Hood

When JavaScript sees a bitwise operator (`&`, `|`, `<<`, `>>`, `^`) it will do an internal conversion from a 64-bit float to a 32-bit int. You can see this by running the following:

```javascript
Number.MAX_SAFE_INTEGER | 0 // Outputs -1
```

What's going on is we're taking the maximum allowed integer value of `9007199254740991` (`0x1FFFFFFFFFFFFF` in hex) and we're doing a bitwise truncation where we keep the last 32-bits (or `0xFFFFFFFF`). For signed integers, `0xFFFFFFFF` corresponds to `-1`, which is what we get as output.

For any of our bitwise operators, we end up with 32-bit signed integers. Though, that's not entirely true. After the bitwise operator is done, our value gets packed back into a JavaScript number. It could stay as a 32-bit signed integer or be turned back into a 64-bit float - it's all up to the JIT. In our case, it will eventually become a 64-bit integer with the current code.

When the multiplication operator is used in JavaScript, it does a 64-bit floating point multiplication. This means that instead of doing integer overflow, we will end up with floating point rounding. To illustrate this, consider `4_294_967_295 * 4_294_967_295`. With signed 32-bit multiplication, we end up with `1`. However, with floating point multiplication, we end up with `18,446,744,065,119,617,000`. This becomes problematic in JavaScript since if we tried to cast the result of the multiplication to an int with a bitwise operator (e.g. `(4_294_967_295 * 4_294_967_295) | 0`) we would end up with `0` instead of `1`.

Then we also have to consider how bit shifts work. Signed right shifts will carry the sign bit over while unsigned right shifts won't carry the signed bit. The bitshift operator (`>>`) in JavaScript/TypeScript will carry over the sign bit. But in the original TinyMT, everything is an unsigned 32-bit where the sign bit is not carried over. So, how do we do that?

### JavaScript's Unsigned Right Shift

We'll start off with the right shift issue since that will help solve most of our issues. JavaScript has an additional bitwise operator which is the bitwise unsigned right shift operator (`>>>`). This operator tells the JavaScript runtime to cast the value to an *unsigned* 32-bit integer, and then do a right shift. This operator will be what powers our entire solution, both since it doesn't carry the sign bit, and since it gives us access to 32-bit unsigned integers.

We can test this operator out by using the it on `Number.MAX_SAFE_INTEGER`.

```javascript
Number.MAX_SAFE_INTEGER >>> 0 // Outputs 4294967295
```

Here we're getting \(2^{32}-1\) which is the maximum value as an unsigned 32-bit integer. We can now start adapting our code to use unsigned integers by adding `>>> 0` after every operation. Here's the new code:


```typescript
class RandomState {
    public status: number[] = (new Array(4)).fill(0);
    public mat1: number = 0x8f7011ee >>> 0;
    public mat2: number = 0xfc78ff1f >>> 0;
    public tmat: number = 0x3793fdff >>> 0;
}
export default class Random {
    private state = new RandomState()

    public constructor(seed: number = defaultSeed()) {
        this.state.status[0] = seed >>> 0
        this.state.status[1] = this.state.mat1
        this.state.status[2] = this.state.mat2
        this.state.status[3] = this.state.tmat
        for (let i = 1; i < minLoop; ++i) {
            const xorL = (this.state.status[(i - 1) & 3] >>> 0);
            const xorR = this.state.status[(i - 1) & 3] >>> 30;
            const mult = (1812433253 >>> 0) * ((xorL ^ xorR) >>> 0);
            const sum = i + mult;
            this.state.status[i & 3] = (this.state.status[i & 3] ^ sum) >>> 0;
        }

        // Debug statement
        console.log(this.state.status)
        // ... rest of code goes here
    }

    // ... Rest goes here
}
```

If we rerun our code, we'll end up with the following state:

```json
[527052801, 401334536, 1079213855, 1584082175]
```

As a reminder, our target state is the following:

```json
[3911076234, 511429786, 2775206926, 3754336954 ]
```

We're getting a bit closer. All of our numbers have the right sign now (previously we had some negative numbers), but we're still off. This is due to us still having the floating point multiplication rather than unsigned integer multilication. So we'll address that next.

### Integer Multiplication in JavaScript

JavaScript introduced a special method which does 32-bit signed integer multiplication called `Math.imul`. We can test out `Math.imul` by using our previous multiplication example where integer and floating point diverged.

```javascript
console.log((4_294_967_295 * 4_294_967_295) | 0) // 0; incorrect
console.log(Math.imul(4_294_967_295, 4_294_967_295)) // 1; correct
```

We now have a way to do 32-bit signed integer multiplication. But how do we do unsigned multiplication? Well, signed and unsigned integer multplication both end up with the same bits; we just have to do a cast between them. Fortunately, our unsigned right shift operator does exactly that. This means we can create an unsigned multiplication method as follows:

```typescript
function multiplyUnsigned(leftHand: number, rightHand: number) : number {
    return Math.imul(leftHand, rightHand) >>> 0;
}
```

We can now update our for loop to be correct.

```typescript
    for (let i = 1; i < minLoop; ++i) {
        const xorL = (this.state.status[(i - 1) & 3] >>> 0);
        const xorR = this.state.status[(i - 1) & 3] >>> 30;
        const mult = multiplyUnsigned(1812433253 >>> 0, xorL ^ xorR);
        const sum = i + mult;
        this.state.status[i & 3] = (this.state.status[i & 3] ^ sum) >>> 0;
    }
```

And we finally get our state output we were looking for:

```json
[3911076234, 511429786, 2775206926, 3754336954 ]
```

## Generating 32-bit Integers

Once we have our state setup, we can generate 32-bit integers with the **temper** function. Porting the C code over is pretty strightforward now that we have our unsigned integers figured out. We do end up with a lot of unsigned right shifts to keep our types correct. Below is the TypeScript code:

```typescript
private temper() {
    let t0 = this.state.status[3] >>> 0
    let t1 = (this.state.status[0] + (this.state.status[2] >>> 8)) >>> 0
    t0 = (t0 ^ t1) >>> 0
    if ((t1 & 1) !== 0) {
        t0 = (t0 ^ this.state.tmat) >>> 0
    }
    return t0 >>> 0
}
```

We are doing an unsigned integer cast at the end of every operation. This is what I found matched the TinyMT C code the best.

At the end of the method we return a 32-bit unsigned integer, so our output range is `0` to `4294967295`.

## Generating 32-bit Floats

Generating 32-bit floating point numbers is trickier. In C, the algorithm is basically as follows:

- Create a union between a 32-bit unsigned integer and a 32-bit float
- Run the 32-bit integer temper algorithm
- Mask the values so we always get bit value which represents a 32-bit float between 1.0 and 2.0
- Assign the value to the unsigned integer part of the union
- Return the float part of the union

Here's the important stuff from the C code:

```c
uint32_t t0, t1, masked;
union {
    uint32_t u;
    float f;
} conv;

// ...
// Assign t0 and t1 similar to our temper method
// ...

masked = /* do some bit operations here */;
conv.u = masked;
return conv.f;
```

For C, that works perfectly since unions share the same location in memory and there isn't any bit changes when accessing different members. However, we're in JavaScript land. There aren't C unions, so we need to do something else.

Fortunately, there is a way to create a 32-bit floating point number from a bunch of bytes in JavaScript. We can do it with a `DataView` and by calling the `getFloat32` method. To make a `DataView`, we'll need a typed array. In our case, we want to create a float from raw bytes, so we'll use a `Uint8Array` for our typed array. However, our typed array also needs underlying storage. For that, we'll use an `ArrayBuffer`. This means our algorithm will look like the following:

- Do the 32-bit integer temper algorithm
- Mask the values as needed and put then in a 32-bit unsigned integer
- Create an `ArrayBuffer` of size 4
- Wrap the `ArrayBuffer` with a `Uint8Array`
- Extract the bytes from our 32-bit unsigned integer and put them in the `Uint8Array`
- Create a `DataView` around for our `Uint8Array`
- Extract a 32-bit float from our `DataView`

Our final code looks like the following:

```typescript
// 1.0 <= result < 2.0
private temperConv() {
    // Do the uint32 temper algorithm
    let t0 = this.state.status[3] >>> 0
    let t1 = (this.state.status[0] + (this.state.status[2] >>> 8)) >>> 0
    t0 = (t0 ^ t1) >>> 0

    // Do our mask
    let u = 0
    if ((t1 & 1) !== 0) {
        u = (((t0 ^ this.state.tmat) >>> 9) |0x3f800000) >>> 0
    }
    else {
        u = ((t0 >>> 9) | 0x3f800000) >>> 0
    }

    // Create our array buffer
    const buffer = new ArrayBuffer(4)

    // Wrap the array buffer with a Uint8Array
    const bytes = new Uint8Array(buffer)

    // Extract our bytes and put them in our Uint8Array
    bytes[0] = (u >>> 24) & 0xff
    bytes[1] = (u >>> 16) & 0xff
    bytes[2] = (u >>> 8) & 0xff
    bytes[3] = u & 0xff

    // Create a data view
    const dataView = new DataView(buffer)

    // Create a 32-bit float and return that
    // (it will get casted to a 64-bit float automatically)
    return dataView.getFloat32(0, false)
}
```

That was a bit painful. The code isn't very easy to follow or read, but it does work and it matches the C code.

## Summary

I won't go into the rest of the code in detail here. There really isn't a lot of value in talking about it since it's mostly a matter of adding unsigned right shfits everywhere. The full code can be viewed on my [git repository](https://gitlab.com/mtolman/test_jam/-/blob/53733ae331b531b81eac96d3ac569ac69dd2697e/js/src/tinymt.ts).

Unsigned integer arithmetic ends up being more complex in JavaScript/TypeScript than in C. If I was doing a lot of work with unsigned integers, I would consider doing it in WASM. WASM has really good support for unsigned integers, and I wouldn't have to clutter code with a bunch of `>>> 0` all over the place.

For small amounts of code, adding in the forced coercions and doing `Math.imul` is probably fine. Just make sure to add some sort of explanation of why you're doing `| 0` and `>>> 0`.

In my personal code, I've gotten used to `| 0` for forcing signed 32-bit integers, and I'm now used to `>>> 0` for focing unsigned 32-bit integers. But when I use those semantics in code touched by other developers, it almost always leads to confusion. Most developers aren't used to seeing JavaScript/TypeScript cluttered with `| 0` and `>>> 0`. Almost always I'm asked, "Why don't you just use `Math.floor`?" Which is a great question since in most cases `Math.floor` is fine. Often we don't need to care about the difference between 32-bit integers and rounded 64-bit floats.

However, when it does matter, it really matters. Knowing how to ensure the correct integer type is important. It's also important to explain is *why* 32-bit integer arithmetic is needed, and why `Math.floor` isn't sufficient. That way other developers will know that they need to take extra steps to preserve the 32-bit integer arithmetic and avoid accidentally introducing 64-bit float arithmetic.

@References
* tinymt
  | type: web
  | link: http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/TINYMT/
  | page-title: TinyMT
  | website-title: Department of Mathematics Hiroshima University
  | date-accessed: 2023-10-28
