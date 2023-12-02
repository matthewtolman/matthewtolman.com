Recently I saw that CUID^[cuid], a way to generate unique IDs, had been deprecated due to security concerns.

That got me really curious. I haven't ever thought of IDs as being "secure." Instead, I've always treated them as untrusted, public-knowledge values. I've always built access controls around my IDs, even when they were randomly generated GUIDs. So I started looking at a lot of ID generators to see if this concept of "secure ids" really did exist, what meant, and what other properties IDs had.

[toc]

## ID Properties

Before we get too much into the IDs themselves, it is useful to talk about the different properties IDs can have. I'll focus on just three properties: Security, monotonically increasing, and temporal.

### Security

There are two main aspects I found for judging ID security:
- How easy is it to predict other ids
- How much information is leaked by the id

Keep in mind that this doesn't mean ids with low security are bad or that they shouldn't be used in production. In fact, many secure apps do use IDs with "low" security. Instead, consider the term "ID Security" as more of a rhetorical device which makes some IDs seem more superior than they are. "Secure" IDs don't make an application secure. Likewise, "insecure" IDs don't make an application insecure. Rather, taking a wholistic approach to security is what makes applications secure. This includes proper access controls, using proper authentication mechanisms, and not trusting user input. A "secure" ID can *mask* underlying security issues, but it does not *fix* them.

### Monotonically Increasing

A monotonically increasing ID is one where IDs can be strictly orderd by the order they were created.

For instance, the following IDs are monotonically increasing:

```text
1 2 3 4 5 6
```

So are the following IDs:

```text
1234-abcd 1234-abce 1235-aaaa 1235-aaab
```

All the above IDs can be sorted to get the same creation order. There are a lot of great benefits to monotonically increasing IDs. They allow programs to reason about the order of events, so a program can know if event 1 or event 2 happened first. For event queues and logs, this is great. They also provide a useful default sorting algorithm. If a user wants to see newest or oldest items first (e.g. like in a timeline), then the database only needs to sort on the ID, which is almost always indexed, even in terribly designed databases.

The downside is that these types of IDs are easy to predict. Also, they can sometimes require extra coordination and synchronization (though that is less rare these days).

### Temporal

Temporal IDs are closely related to monotonically increasing IDs. The difference is that they have a timestamp embedded into them - usually in the highest digits of the ID. They aren't guaranteed to be in creation order (usually when multiple IDs are created simultaneously), but they do often require less coordination.

The biggest benefit to temporal IDs is that they allow searching and sorting based on time of creation just using the ID field. For instance, if your ID is of the format YYYYMMDD-xxxx (where `x` is random), then you can find any IDs created in December 2020 with the query:

```sql
SELECT * FROM mytable WHERE id >= '20201201-0000' AND id <= '20201231-9999'
```

The downside is that these IDs are also generally pretty easy to predict. They also aren't strict on preserving creation order, so there is some fuzziness there - especially as a system scales in the number of records generated. This means that temporal IDs have a looser ordering than monotonically increasing IDs. For most use cases it's fine, though sometimes it can matter.

## Categories of IDs

Most IDs fall into generalized categories of IDs where they share properties with one ore more other IDs. There are a few solitary ID generation methods, but for the most part I'll talk about them in groups.

### Auto Incrementing IDs

Auto-incrementing IDs are the bread and butter of ID generation. Each record gets a unique number assigned to it, and the numbers are always assigned in ascending order. Generally this works by having an `id` field in an SQL table with type `INT` and properties `NOT NULL AUTO_INCREMENT`.

Auto incrementing IDs are *monotonically increasing* IDs. They are also super easy to predict since they increase by a fixed amount. From a security perspective, these are the "least secure" and are almost always used when someone argues for more "secure" IDs. But usually when these IDs are used as an example of "bad security," the real issue is either broken access controls or improper authentication mechanisms, not the IDs themselves.

That's not to say they are without flaws. Auto Incrementing IDs do require a central point of synchronization to create new IDs (usually the database). They also do leak information about how many records have been made, which can be used to determine how much usage a platform is experiencing. For multi-tenant systems with proper isolation, this only matters for the tenant and user identifiers since the rest of the IDs are local to a tenant. But for systems without isolated tenants (e.g. social media), this can become problematic since it allows competitors to guage how effective their marketing is at taking away site traffic.

### Purely Random IDs

The next category of IDs are purely random ids. These are the next simplest ID as they just require generating random bytes and then encoding them into text. Among random IDs there are [UUIDv4](https://datatracker.ietf.org/doc/html/rfc4122#section-4.4) and [Nanoid](https://github.com/ai/nanoid). 

Generally, these IDs are considered very secure since there is little to no correlation between two different IDs, so they are hard to predict. They also don't leak any information about the host. They are also seen as requiring little or no synchronization since there really isn't a way to synchronize randomness. Though, there is the possibility of collisions though where an ID that gets generated matches an ID that was previously generated, so having retry logic can be needed.

The one caveat with these IDs is that they need a properly initialized cryptographic library to be truly unpredictable. If a normal random number library is used (e.g. `Math.random`, `rand`, etc) then these IDs drop off in quality drastically. Usually this means they become predictible, and there is also a higher chance of collisions.

For virtualized environments (VMs and containers), there can be additional hurdles in getting cryptographic random number generators setup. Cryptographic random number generators rely on a source of entropy, which is usually created by tracking all the messy, dirty, chaotic nature of the operating environment. Virtualized environments are meant to be isolated from the environment, which means they often have a sterilized entropy pool which needs to get filled up^[exoscale-prng-vm]. Sometimes the seeding happens by "passing through" the randomness from the host. However, when randomness is "passed through", it's really getting divided between the host and all virtual environments, which can lead to entropy starvation^[vmware-randomness].

As a workaround, some companies will manually feed entropy into their programs from a truly random source^[cloudflare-lavarand]. This requires extra setup and knowledge to get working properly, but once it's working then services will have consistent access to random numbers.

### Timestamp and Random IDs

Timestamp and random ids work by having part of the ID represent the current time (usually based on the Unix time) followed by a random value for the rest of the ID. Often the time portion is in seconds or milliseconds. IDs which fall in this category include [ulid](https://github.com/ulid/spec), [ksuid](https://github.com/segmentio/ksuid), and Firebase's [PushId](https://firebase.blog/posts/2015/02/the-2120-ways-to-ensure-unique_68/).

One caveat is that the "random" part is only sometimes only somewhat random. Sometimes these types of IDs are meant to be monotonically increasing as well as temporal. This is done by "incrementing" the random bit for every ID created in the same "instant" (second/millisecond). So if we generated 4 IDs in timestamp `12345` and 3 IDs in timestamp `12346`, we'd end up with something like the following:

```text
12345-5938
12345-5939
12345-5940
12345-5941
12346-1469
12346-1470
12346-1471
```

The hybrid "random/counter" model does make those IDs more predictible, but it does make them monotonically increasing. The ulid and PushId specifications do this. Meanwhile ksuid doesn't have a built in counter, so it's only temporal.

### Timestamp, Counter, and Fingerprint

The next main approach is to combine the current time, a monotonically increasing counter, and a machine fingerprint. Some implementations of this pattern are [ObjectId](https://www.mongodb.com/docs/manual/reference/method/ObjectId/), [xid](https://github.com/rs/xid), [Sonyflake](https://github.com/sony/sonyflake), and the historical [Twitter Snowflake](https://github.com/twitter-archive/snowflake/releases/tag/snowflake-2010) (I just read the 2010 spec back when it was called Twitter).

Each machine is assigned some sort of ID or fingerprint. In ObjectId, this is done randomly. In Sonyflake it's based on IP. In Snowflake it's done through a central configuration service. For xid it's based on the host id from the OS and a process id.

The counter portion usually starts from a random value and increases by one each time an ID is generated. The counter also rolls over to 0 once the maximum value is exceeded.

These IDs are both temporal and monotonically increasing. However, there is one more disadvantage which makes these less secure: they leak infrastructure details.

Sonyflake uses the bottom half of IP addresses as a fingerprint, so every ID leaks network topology. Snowflake uses a centralized service to assign fingerprints, so every ID leaks which machine it was made on. Xid uses the OS host ID and process ID, so every ID leaks host and process information. ObjectId uses a random identifier that lives for the lifetime of the process, which means it leaks process restarts. All of these fingerprints also leak how many machines are running.

That doesn't necessarily mean fingerprinted IDs should be avoided (ObjectId is used by MongoDB, a very popular NoSQL database). Instead, it means developers need to be aware of what is being leaked and decide if the ID is leaking too much information.

### CUID

Now we get to an ID which combines everything up to this point. It has a timestamp, a counter, a fingerprint (based off the hostname and PID), and a random value. It's also the ID type which started this whole article. The [CUID](https://github.com/paralleldrive/cuid) feels like the kitchen sink, and it's the only ID I could find with this many different types of parts. The CUID also includes a letter prefix, which means this is the first ID which can always be used as an identifier in any programming language (this includes HTML entity IDs).

It seems like a great idea, and I thought so too considering I made an [implementation a few years ago](https://gitlab.com/mtolman/cuid) and then updated the CMake configuration earlier this year. But this ID has been marked deprecated by its creators for security reasons.

Why? The creators claim it is insecure because it is predictible since it is a temporal, monotonically increasing ID. Which raises the question: If that's an issue, why have none of the other temporal and monotonically increasing IDs been marked as deprecated?

We've looked at PushId from Firebase (owned by Google), ObjectId from MongoDB, Sonyflake from Sony, and auto incrementing IDs from every SQL database. Why does CUID get a big "deprecated" warning for security while the rest don't?

From what I can tell, the answer is that Parallel Drive (the account behind CUID) claimed that CUID was "secure," and "unpredictible," but no one else made similar claims for their IDs. This means that what Parallel Drive wanted and what they had were two different things. So when they found out CUID wasn't actually unpredictible, they deprecated it in favor of a more unpredictible alternative. That doesn't mean CUIDs are bad, just that they are temporal, monotonically increasing IDs which can be predicted and also leak infrastructure details. It's pretty much the same boat as the IDs in the previous section.

So let's take a look at the successor and see how they changed CUID.

### CUID2

[CUID2](https://github.com/paralleldrive/cuid2) is the successor to CUID. Reading through the code, it's almost identical to CUID with one major change: it passes the ID through a SHA3 hash before returning it.

This actually makes sense for being the biggest change. CUID was getting new commits listing new ports through mid-December 2022^[cuid-port], only to get deprecated December 30, 2022^[cuid-dep]. CUID2 was started December 26, 2022^[cuid2-start]. That doesn't leave a lot of time for redesigning an ID from scratch, so a lot was borrowed from CUID.

The nice thing about CUID2 is that it does hash the details which were previously leaked. The fingerprint, timestamp, and counter are all hashed. Since it still has all the collision resistant elements underneath, it requires little to no coordination.

The flipside is that it's now a lot similar to a random ID. The ID is no longer temporal or monotonically increasing. That could be good for developers who've never heard of CUID, but for those who previously were using CUID, then losing those attributes could be a tough pill to swallow.

## My Thoughts

I don't think there is any "best" ID. Having proper access controls should definitely be done. If you are using a value for authorization and/or authentication (e.g. password reset, API access, etc), then definitely use a token (e.g. signed JWT, single-use random value from a cyrptographic random number generator, etc). Don't use IDs where you should be using tokens, it'll cause a lot of issues.

As for why I don't consider "predictibility" to be an issue, that's because IDs are actively sent to the client all the time. They are sent in API responses, HTML content, URLs, and more. If there is a table of users, that table usually reveals the IDs of every user in that table. If there's an API, it gives and takes IDs. These IDs can include any of the IDs listed here, or they can include handles and usernames (which if they can be used for CRUD operations, they are effectively an ID).

IDs also end up on log servers and in log files, which usually have different (often lower) security requirements than the application servers. Often these log messages are transmitted in plain text as part of bug reports, task comments, messages, emails, and more. They are also often integrated into error monitoring tools - including alerts, dashboards, and graphs. Sometimes log files are stored on the client's machine and have to be transmitted to the developers. Logs should be treated as insecure (aka. don't log anything sensitive), and if IDs are getting logged then they should also be treated as insecure.

The other side of my reasoning is that since IDs are public, any information they leak is public too. If I don't like what inferences can be gathered from my IDs, then I need to change my IDs. This is why I give more weight to what a fingerprint entails rather than how predictible an ID is. For instance, I don't use Sonyflake since it leaks half of my IP address; I also don't use CUID in production environments since it leak part of my hostname. I do use CUID for local debugging of personal projects since it means I don't need to setup a full logging library to get time and host information in my print statements (I also added thread IDs to my own implementation so I can quickly know which thread is printing). But my use case for CUID is really niche, so I'm not shedding tears over it's deprecation.

As for CUID (which started this whole article), I can understand why it was deprecated. The orignal creator was claiming that it was unpredictible when it actually wasn't. Instead of just silently removing that section or outright deleting the specification after the community ported it over 20 times, the creator decided to instead deprecate it and create a new ID specification. The creator then shifted attention to the newer ID which is actually was what the creator wanted in the first place. I personally think it was a good choice on the creator's part.

Going forward, I am going to start taking a look at Nanoid and KSuid more. I'm not that interested in CUID2 since my main interest in CUID was the timestamp and host fingerprint. I was mostly interested in the host fingerprint when I was playing with docker containers and needed to know which container had issues at a glance. Now I'm not using docker nearly as much, so the fingerprint is less useful to me.

Nanoid is nice since it's really easy to implement (easier than UUIDv4), so I can quickly create it in any programming language. Since I'm wanting to experiment with some newer programing languages that don't have a lot of libraries, being able to quickly create a consistent ID format will be nice. KSuid seems nice since it is temporal, so I can still keep my time information that I was getting with CUID. It also doesn't seem to be that hard to implement, so I can also recreate it in newer languages.

@References
* cuid
  | type: web
  | link: https://github.com/paralleldrive/cuid
  | page-title: CUID
  | website-title: GitHub
  | date-accessed: 2023-11-14
* exoscale-prng-vm
  | type: web
  | link: https://www.exoscale.com/syslog/random-numbers-generation-in-virtual-machines/
  | page-title: How to ensure entropy and proper random numbers generation in virtual machines
  | website-title: Exoscale Blog
  | author: Cédric Dufour
  | date-accessed: 2023-11-30
* vmware-randomness
  | type: web
  | link: https://tanzu.vmware.com/content/blog/challenges-with-randomness-in-multi-tenant-linux-container-platforms
  | page-title: Challenges With Randomness In Multi-tenant Linux Container Platforms
  | website-title: vmware Tanzu Blog
  | author: James Bayer
  | date-accessed: 2023-11-30
* cloudflare-lavarand
  | type: web
  | link: https://blog.cloudflare.com/randomness-101-lavarand-in-production/
  | page-title: Randomness 101: LavaRand in Production
  | website-title: The Cloudflare Blog
  | author: Joshua Liebow-Feeser
  | date-accessed: 2023-11-30
* cuid-port
  | type: web
  | link: https://github.com/paralleldrive/cuid/commit/c107cfe746639ec47cb73a46934bd64218430971
  | page-title: Commit task: reference .NET implementation of cuid
  | website-title: GitHub
  | date-accessed: 2023-12-01
* cuid-dep
  | type: web
  | link: https://github.com/paralleldrive/cuid/commit/e15b0679a2f3d9444021e584f8cfffbc7ee54ce8
  | page-title: Commit: Update README
  | website-title: GitHub
  | date-accessed: 2023-12-01
* cuid2-start
  | type: web
  | link: https://github.com/paralleldrive/cuid2/commit/39e8e107b31249d8f1f06388816861ca6c3e0f66
  | page-title: Commit: Initial Commit
  | website-title: GitHub
  | date-accessed: 2023-12-02
