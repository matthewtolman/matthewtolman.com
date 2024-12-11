Recently, I was using Zig and was trying to avoid memory allocations. My program was generating data based on user input, but I needed a way to format the data for use in other programs. I was also only keeping one record in memory at a time to limit memory usage. So, I wanted a data format that could write a single record at a time, and I wanted to write to a writer so I could write straight to a socket or disk as needed. I also wanted to avoid introducing memory allocations while serializing.

The first thing I did was look at the Zig standard library, but I couldn't find anything that fit my needs. I also looked at some libraries, but didn't find anything there. So [I wrote my own CSV encoder](https://github.com/matthewtolman/zig_csv/blob/82731a2d82ccc37a3332c8bd96daa145a149f1fa/src/writer.zig), which wasn't very hard. But, once I got a writer I thought "how hard is it to write a fast CSV parser?" That question turned into a much bigger project, so let's dive in!

## Creating a baseline

To start off, I decided to create a simple parser I could use as a performance baseline. For my baseline, I kept it simple and operated on an array of bytes instea of operating on a reader. I also didn't decode CSV values. Instead, I returned a slice pointing to the location of each field in the input bytes. I also used an iterator so I could parse row by row instead of parsing the entire CSV file. This let me lazily parse only what I needed while also minimizing memory usage. This became what I called my ["raw" parser](https://github.com/matthewtolman/zig_csv/blob/82731a2d82ccc37a3332c8bd96daa145a149f1fa/src/raw.zig).

Overall, this did well. I was able to parse a 12MB file in **57ms** in release mode on my old Intel-based MacBook Pro.

> Note: For all benchmarks, I'm using the same machine, same release config, and I'm pre-loading the file into memory.

Below is a usage example of my parser:

```zig
const csv =
    \\userid,name,age
    \\1,"Johny ""John"" Doe",23
    \\2,Jack,32
;

// No allocator needed
var parser = zcsv.slice.init(csv);

// Will print:
// Row: Field: userid Field: name Field: age 
// Row: Field: 1 Field: "Johny ""John"" Doe" Field: 23 
// Row: Field: 2 Field: Jack Field: 32 
//
// Notice that the quotes are included in the output
while (parser.next()) |row| {
    std.io.debug("\nRow: ");

    // Get an iterator for the fields in a row
    var iter = row.iter();

    // Iterate over the fields
    while (iter.next()) |f| {
        std.io.debug("Field: {s} ", f);
    }
}

// Detect parse errors
if (column.err) |err| {
    return err;
}
```

Note that you do need to check the parser if there was an error. This pattern does come from Go where some iterators need to be checked after the loop. If someone knows of a more "Zig" way to do this, let me know on [Ziggit](https://ziggit.dev/t/zcsv-csv-parsing-for-zig/6497/3).

## Moving to a reader

Next I wanted to move from parsing a CSV file in memory to parsing a file from a reader. A reader would allow me to stream a file from disk or from a socket. Ideally, this would allow for parsing much larger CSV files.

Unfortunately, readers don't have a memory address I could use as a return result. I would need some other place to put parsed field data. I decided to achieve this by writing each field to a writer. This did mean that each field was being copied to somewhere, but that "where" wasn't a concern of my parser. This also gave the user of the paresr way more control over memory since they could use either an allocating or non-allocating writer depending on their needs.

There was one flaw with this approach though. I can only send individual bytes through a writer. This meant I would either need to have a delimiter between fields (and another parser) to find the fields in a row, or I would need to move from parsing rows to parsing fields. I chose the later. I also added a flag to the parser to indicate whether the last field parsed was the last field on a row.

This became my ["field stream" parser or "stream" for short](https://github.com/matthewtolman/zig_csv/blob/82731a2d82ccc37a3332c8bd96daa145a149f1fa/src/stream.zig). It was able to parse the same 12MB file in **100ms**. I don't know where the slowdown was (e.g. reader interface, copy, decoding, etc). However, what I did was make this my second baseline for any reader-based parsers.

Below is example usage code:

```zig
// Get reader and writer
const stdin = std.io.getStdIn().reader();
const stdout = std.io.getStdOut().writer();

// no allocator needed
var parser = zcsv.stream.init(stdin, @TypeOf(stdout), .{});

// If given same CSV as previous example, will print:
// Row: userid name age 
// Row: 1 Johny "John" Doe 23 
// Row: 2 Jack Field: 32 
//
std.debug.print("\nRow: ", .{});
while (!parser.done()) {
    try parser.next(stdout);
    // Do whatever you need to here for the field

    if (column.atRowEnd()) {
      // end of the row
        std.debug.print("\nRow: ", .{});
    }
}
```

## Improving the developer experience

So far, all of my parsers were zero-allocation parsers. They never allocated memory to do the actual parsing. But they were also clunky to use. The raw parser required a separate decoding step for the user, and the field parser required checking if you were at the end of the row. These weren't huge hurdles, but they were hurdles. However, for most use cases we just want to load a small CSV file in an easy-to use way. For these use cases I created allocating [column](https://github.com/matthewtolman/zig_csv/blob/82731a2d82ccc37a3332c8bd96daa145a149f1fa/src/column.zig) and map parsers.

> Note: the above link is for the parser post speed improvements as the faster code has been merged to main. [Here's the link for the original, slower code](https://github.com/matthewtolman/zig_csv/blob/1670748a7f9f2c3b94d82693e7b8024fe47729c2/src/column.zig).

My allocating parsers used the field stream parser under the hood. The biggest difference is they sent the field outputs to an `ArrayList`, which is a dynamically resizeable array. The column parser had an array list of these array lists, and it would just append a new item for every field in a row. The map parsers took the fields and inserted them into a hash map (also dynamically allocated). This then meant that the caller was either iterating over an array list of fields or a map of fields.

This gives us the following experince for the column parser:

```zig
// Get an allocator
var gpa = std.heap.GeneralPurposeAllocator(.{}){};
defer _ = gpa.deinit();
const allocator = gpa.allocator();

// Get a reader
const stdin = std.io.getStdIn().reader();

// Make a parser
var parser = zcsv.column.init(allocator, stdin, .{});

// Iterate over rows
while (parser.next()) |row| {
    // Free row memory
    defer row.deinit();

    std.debug.print("\nROW: ", .{});

    var fieldIter = row.iter();
    // Can also do for(row.fields()) |field|
    while (fieldIter) |field| {
        // No need to free field memory since that is handled by row.deinit()

        // .data() will get the slice for the data
        std.debug.print("\t{s}", .{field.data()});
    }
}

// Check for a parsing error
if (column.err) |err| {
    return err;
}
```

One interesting thing to note about lifetimes is that lifetimes are tied to the row, not the field. Since I already had to put the field memory into an array list held by the row, I decided to just have the row clean everything up.

My map parser ended up being similar. It also used the column parser internally. But, there was one little caveat. The maps parsers needed to keep the first row in memory so it knew what to use for the keys. This memory had to be owned by the parser, so the parser needed a `deinit` method. It also posed a lifetime question for the key memory of the returned map. Should the map own it's key memory or should the parser? I chose both, and created a [map_ck](https://github.com/matthewtolman/zig_csv/blob/82731a2d82ccc37a3332c8bd96daa145a149f1fa/src/map_ck.zig) which copies the keys for each row, and a [map_sk](https://github.com/matthewtolman/zig_csv/blob/82731a2d82ccc37a3332c8bd96daa145a149f1fa/src/map_sk.zig) which shares key memory with the parser.

Below is a usage example of the map parsers (it works for both, just swap `map_sk` to `map_ck`):

```zig
// Get an allocator
var gpa = std.heap.GeneralPurposeAllocator(.{}){};
defer _ = gpa.deinit();
const allocator = gpa.allocator();

// Get a reader
const stdin = std.io.getStdIn().reader();

// Make a parse
var parser = try zcsv.map_sk.init(allocator, stdin, .{});
// Note: map parsers must be deinitialized!
defer parser.deinit();

// We're building a list of user structs
// User struct not shown here for brevity
var res = std.ArrayList(User).init(alloc);
errdefer res.deinit();

// Iterate over rows
while (parser.next) |row| {
    defer row.deinit();

    // Validate we have our columns
    const id = row.data().get("userid") orelse return error.MissingUserId;
    const name = row.data().get("name") orelse return error.MissingName;
    const age = row.data().get("age") orelse return error.MissingAge;

    // Validate and parse our user
    try res.append(User{
        .id = id.asInt(i64, 10) catch {
            return error.InvalidUserId;
        } orelse return error.MissingUserId,
        .name = try name.clone(allocator),
        .age = age.asInt(u16, 10) catch return error.InvalidAge,
    });
}
```

Now time for the interesting bit. Performance! Here are the numbers for the same file:

* **column** - 3,368ms
* **map_sk** - 4,081ms
* **map_ck** - 4,175ms

### Improving performance

Ouch! Those were really slow. However, there's something important to note. The map parser which shares memory was 100ms faster than the parser which copied memory. This is because it did one less allocation and one less copy per row. With that realization, I started looking for ways to cut back on copies and allocations.

My search then led me to one of the most important sections of the column parser. Here's the section of code I looked at:

```zig
var row = Row{ ._data = std.ArrayList(Field).init(self._allocator) };
row.deinit()

/// ... several lines excluded to remove noise

while (!at_row_end) {
    // Clean up our field memory if we have an error
    var field = Field{
        ._data = std.ArrayList(u8).init(self._allocator),
    };
    errdefer field.deinit();
    try self._buffer.next(field._data.writer());
    // try adding our field to our row
    try row._data.append(field);
    at_row_end = self._buffer.atRowEnd();
}
```

Here I'm doing, at minimum, one allocation per row, followed by one allocation per field of the row. Except, that's not entirely true. I'm also doing additional allocations and __copies__ everytime either the row or a field array list runs out of memory. There is no upper bound on how many times these additional copies happen (other than the RAM in your computer).

My goal was to reduce the number of string copies. The way I chose to go about it was to replace the row data with two array lists, one which was a big chunk of bytes holding all the field memory, and another being a list of indexes into that chunk. I also kept track of the maximum number of columns seen, and the maximum row length seen. I used those metrics as my initial guess into how much allocation I would need to do upfront.

> An alternate approach would be to track the number of columns and the size of columns. At the time I didn't do that since my logic was that columns differ a lot in length from each other (especially when some columns are lenght 0 while others in the same row may be length 50). However, I noticed that most rows were of similar length. My thought process was that by aggregating the columns into one chunk of memory I could get better memory utilization. I didn't make a version that just preallocates the field memory to compare to, so I don't know if my hunch was the case. But, I'm satisified with the performance I do have, so I'm not going to sweat going back in and checking.

My code then became like the following

```zig
var row = Row{
    // Keep track of the indexes into the byte array
    // Using indexes. RowField was just two usizes (position and length)
    // Not using pointers into bytes since resizing the array invalidates pointers
    ._fields = std.ArrayList(RowField).init(self._allocator),
    // Actual memory
    ._bytes = std.ArrayList(u8).init(self._allocator),
};

// Doing this defer since we don't want to access a deinitialized
// row when there's an error
defer {
    self._row_field_count = @max(self._row_field_count, row._fields.items.len);
    self._row_byte_count = @max(self._row_byte_count, row._bytes.items.len);
}

errdefer row.deinit();

try row._fields.ensureTotalCapacity(self._row_field_count);
try row._bytes.ensureTotalCapacity(self._row_byte_count);

while (!at_row_end) {
    const start = row._bytes.items.len;
    try self._buffer.next(row._bytes.writer());
    const field = RowField{
        ._pos = start,
        ._len = row._bytes.items[start..].len,
    };
    // try adding our field to our row
    try row._fields.append(field);
    at_row_end = self._buffer.atRowEnd();
}
```

Let's rerun our code and see the savings.

- **unoptimized** - 3,368ms
- **optimized** - 2,041ms

Not bad.

## Gotta go fast

We're not done yet. So far we've been processing one byte at a time. But that's not the only way to do it. What if we processed multiple bytes at a time? Let's rewrite our raw parser with SIMD (again, get our algorithm down before doing the reader).

Well, Zig introduces the `@Vector` compiler directive which lets us use SIMD instructions. We can also start using SIMD algorithms. The most useful algorithm I found is from [Chris Wellons for finding quoted segments of a CSV file](https://nullprogram.com/blog/2021/12/04/). It's a great read, and I highly recommend it.

The gist of it is that we compare our chunk of bytes to the double quote character, convert those comparisons to a bit mask (e.g. `0010 0110 0010 0000`). We can then take our bitmask and subtract 1 to get the section of the first "post-quote" region (e.g. `0010 0110 0001 1111`). We can and that with the first region to clear it and subtract 1 again to get the second region (e.g. `0010 0010 1111 1111`). If we keep doing that, we'll end up with a series of masks showing the regions that follows a quote. We can turn these masks into toggles, with the first mask turning "on" a quoted region, the next mast turning "off" a quoted region, the third turning "on", and so forth. We can get this toggling behavior by xoring everything together. There's also a segment for "carry over" when we end inside a quoted region (basically we add a mask region of "all ones" to our list). For exact specifics, read Chris' post.

Chris' post doesn't cover how to fully parse a CSV with this technique (he was instead interested in simply knowing which bytes to replace for a processing pipeline). However, it gives us the fundamentals we need to make our parser.

There are only a few main characters we need to handle. The comma (`,`), carriage return (`\r`), newline (`\n`), and double quote (`"`). Chris' post gives us how to detect and handle quoted regions. So now all we need to do is create our matching bitmasks for the other characters. Once we have that, we can mask out the quoted versions by anding with the inverted quote mask (`comma_mask & ~quoted_regions`). Then we can use `ctz` (count trailing zero) to know where the end of the field is (aka. where the next delimiter is). If there's no end of the field, then we simply assume that the entire chunk is part of a the field and write it all out.

Unfortunately, Zig doesn't handle explicit SIMD very well (yet). When we do our comparison, we get back a `@Vector(_, bool)`. But, Zig doesn't [allow bitwise operators on a `@Vector(_, bool)`](https://github.com/ziglang/zig/issues/14306k). The workaround is to unpack our vector into an unsigned integer. I couldn't find a built-in way to do that, so I used bitwise operators. For the size of the vector, I did 64 to map to a u64. Here are the numbers:

* `original` - 57ms
* `simd` - 54ms

That doesn't seem right.

### SIMD gotcha

What's going on? Well, it turns out that `@Vector` will only use SIMD if, and only if, the target hardware has a SIMD register and instruction to support the operation. Otherwise, it will silently fallback to using a non-SIMD loop. No warnings or errors. My computer wasn't able to properly handle a comparison on a `@Vector(64, u8)`, so I got a slow loop.

It's unfortunate there aren't warnings. If I wasn't measuring, I would have assumed I was getting a performance gain when in reality I was getting a silent performance issue. I message of "Vector type 'u8' not natively supported" or "Vector size too large" would have gone a long way.

Instead of fixing my SIMD code, I replaced it with a loop that compared each character to a target character and saved a bit into a u64 directly. By switching from `@Vector` to a loop I was able to get my "simd" time down to **20ms**. Plenty fast.

### Minor gains

Once I got my SIMD algorithm figured out, I adapted it to the reader/writer pattern. The final time for the adapted SIMD pattern was **25ms**. And that time is with field decoding.

I then switched my allocating column parser from using the (slower) field stream to the new SIMD field stream to save around 75ms (out of 2 full seconds).

## Wrap Up

This was my first time writing a SIMD parser. It was a fun experience, especially since there are some weird edge cases (like how do you handle look-ahead across chunk boundaries?). It took some getting used to thinking about how to use bitwise operators for parsing. This was also a good exercise to use bitwise operators in more depth than I had done previously.

I also was surprised how much memory allocations can slow a program down. I had always known that it was slower to allocate memory than to use the stack, but I hadn't internalized how much slower it was. My allocating parsers were never able to get below a single second for my 12MB test file. However, my non-allocating parsers were never above a second. The amount of time spent in the allocator dwarfed any improvements I got from SIMD parsing.

It was also a good realization for me that I don't need to use SIMD intrinsics or raw assembly instructions to take advantage of SIMD techniques. Something as simple as precomputing bitmasks and then doing bitwise operations on those can be advantagous enough to increase performance.

The full code is posted on [my GitHub](https://github.com/matthewtolman/zig_csv).

