Recently I went down a rabbit hole looking at different ways of creating globally unique identifiers (GUIDs). I had previously seen a couple different methods, and I had implemented some of them in the past. But what really got my attention was when one of them was deprecated by it's creator for "security issues."

That got me really curious. I haven't ever thought of an id as being "secure." I've always thought of them as untrusted, public-knowledge values where I have to build access control around. The primary concern from the creator was in regards to predictabilty. They felt their ID method was too predictable to be used safely. I wanted to know how the predictability compared to other methods, and if it was even an issue to worry about.

To start off, we'll first take a look at a bunch of different GUIDs. I've broken them into different categories so we can talk about several of them at once. At the end I'll talk about the deprecated ID, why it was deprecated, and what my thoughts are.

[toc]

## Purely Random IDs

The first category of GUIDs I found are purely random ids. These IDs include [UUIDv4](https://datatracker.ietf.org/doc/html/rfc4122#section-4.4) and [Nanoid](https://github.com/ai/nanoid). How these IDs work is some kind of PRNG (usually a cryptographically secure PRNG) is used to generate a lot of random bytes. Those bytes are then encoded into some sort of text format (e.g. Base64 for Nanoid). Sometimes additional formatting is added in (e.g. dashes and version/variant info for UUIDv4).

These are by far the simplest GUID algorithm. The predictability of these ids depends entirely on the PRNG used. Non-cryptographically secure PRNGs are going to be more predictable than cryptographically secure PRNGs. But that's not the end of the story. How a cryptographically secure PRNG is seeded also determines the predictability.

Computers aren't really all that good at getting truly random values, so they try to collect random data from hardware through a combination of methods, such as page faults, interrupts, context switches, etc. Unfortunately, this tends to fall apart for virtualized machines due to how isolated they are from "the real world"^[exoscale-prng-vm]. The issues get exacerbated with multiple VMS or containers as they compete for the entropy generated from the host machine^[vmware-randomness].

As a workaround, some companies will manually feed entropy into their services from a truly random source^[cloudflare-lavarand]

@References
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
