[toc]

## Introduction

One of the most under-taught but highly important aspects of software development is localization.
We live in an increasingly globalized world. As such, businesses who want a global audience need to
localize their products and websites for different regions and languages, especially since English
speakers make up about a quarter of the internet's user-base^[user-language].

However, localization isn't an easy task to undertake. The primary hurdle is making sure that code
can be adapted for different languages. Unfortunately, when there are hundreds of human
languages^[linguistics-number-of-languages], it's highly likely that the majority of languages act
nothing like the languages spoken by developers on any given product. Consequently, subtle
anti-patterns and incorrect assumptions about linguistics will creep their way into the codebase.
While these assumptions will go unnoticed when the product is released in the developer's primary
language, they will rear their ugly heads once the company tries to take the product global. The
aim of this post is not to be a comprehensive guide to all assumptions and anti-patterns that
developers make. Rather, it is to lay the foundation for thinking about linguistics in code and
for showing how to address several of the most common text anti-patterns. Future
articles will cover other aspects of localization.

## Separate Text from Code

The first key to localization is to isolate code from the influence of grammatical rules.
Without this isolation, any attempt at localization will require mutating and complicating logical
rules inside of code to fit the grammars of different languages.

For example, consider the following code:

```javascript
// main.js
const response = await fetch('/api/current_user');
const currentUser = await response.json();
const content = document.getElementById('content');
content.innerHTML = "Hello " + currentUser.fullName + (currentUser.isAdmin ? ", you're an admin!" : "!");
```

A few things are going on in the above code which makes localization difficult. The first is problem
is that the text displayed is stored inside the JavaScript code. The reason this is problematic
is that we have to create and maintain copies of our codebase for every language we want to support.
This means that if I want a Spanish version of the codebase, I need to create a JavaScript file
that looks like the following:

```javascript
// main-es.js
const response = await fetch('/api/current_user');
const currentUser = await response.json();
const content = document.getElementById('content');
content.innerHTML = "¡Hola " + currentUser.fullName + (currentUser.isAdmin ? ", usted esta un administrador!" : "!");
```

Do note there are problems with the above translation, which we'll get to in a bit. For now, let's
focus on the current issue of coupling code with text. Already, we have two different files with
different texts. If we wanted to add a new feature, we'd have to update both files to have the
new feature. Alternatively, we could try wrapping everything into if-else statements, but before
we go that far let's add one more language to the mix to see how things change, in this case we'll
add Chinese. Here is the Chinese version of the file.

```javascript
// main-zh.js
const response = await(fetch('/api/current_user'));
const currentUser = await response.json();
const content = document.getElementById('content');
content.innerHTML = currentUser.fullName + '您好' + (currentUser.isAdmin ? ",  您是管理者！" : '！');
```

One thing to note about the Chinese version of the file is that words are starting to change order.
In Chinese, the name comes before the greeting, so our code has to change to accommodate this. We'll
talk more about word order in the next section. The important thing to keep in mind for this section
is that if we were to keep text inside our code, we will have to change our code logic for different
languages. This makes maintaining all of our language files much more difficult since we are coupling
the grammar of the target language with the structure of our code.

Let's try to make a combined file for all three languages. Our combined file would look something
like the following.

```javascript
// main.js
const response = await fetch('/api/current_user');
const currentUser = await response.json();
const content = document.getElementById('content');
const locale = detectLocale();
if (locale === 'es') {
    content.innerHTML = "¡Hola " + currentUser.fullName + (currentUser.isAdmin ? ", usted esta un administrador!" : "!");
}
else if (localze === 'zh') {
    content.innerHTML = currentUser.fullName + '您好' + (currentUser.isAdmin ? ", 您是管理者！" : '！');
}
else {
    content.innerHTML = "Hello " + currentUser.fullName + (currentUser.isAdmin ? ", you're an admin!" : "!");
}
```

Already we can see that the code is starting to get cluttered with many if and else
statements. We also have a lot of logic repeating itself, but sometimes with small variations which
can make it hard to tell if there's a bug or not. These types of branch statements also have to
exist anywhere that text is shown to the user. If a product needs to support more than one or two
languages, this quickly becomes unsustainable.

The alternative solution is to separate text out of the code logic and replace text with a single
function call. The resulting code would look something like the following:

```javascript
// main-intl.js
const intl = getTranslationEngine(); // this is a placeholder for getting an instance whichever internationalization library you use
const response = await fetch('/api/current_user');
const currentUser = await response.json();
const content = document.getElementById('content');
content.innerHTML = intl.localize('user.greet') + currentUser.fullName + (currentUser.isAdmin ? intl.localize('user.isAdmin') : intl.localize('user.isNotAdmin'));
```

Our text pieces would then be stored in some sort of data file. Generally, each langauge is stored
in a separate file, that way when code loads files its only loading the language it needs.
Below is what the English JSON file could look like:

```json
{
  "user.greet": "Hello ",
  "user.isAdmin": ", you're an admin!",
  "user.isNotAdmin": "!"
}
```

The code is now much clearer. However, our current code lost the ability to represent the word
order change in Chinese. Fortunately, almost every localization library provides a mechanism to
support word order changes, which I cover in the next section.

## Languages and word order

One of the main differences between different languages is that they present information in different
orders. Consider the sentence "~span::style=color:blue::They::~/span:; ~span::style=color:purple::drove ::~/span:; ~span::style=color:green::up the mountain::~/span:; ~span::style=color:#a48b07::last night::~/span:;."
In Chinese, the sentence becomes "~span::style=color:blue::他们::~/span:;~span::style=color:#a48b07::昨晚::~/span:;~span::style=color:purple::开车::~/span:;~span::style=color:green::上山::~/span:;。"
For convenience, I've highlighted the different corresponding parts of the two sentences.
The subject ("they") is blue. The verb ("drove") is purple. The object ("the mountain") is green.
The time ("last night") is dark yellow.
In English, the word order is subject, verb, object, time.
In Chinese, the word order is subject, time, verb, object.
This reordering can have surprising consequences once we start substituting pieces of a sentence.
If we substituted values with string concatenation, then we would need to change the concatenation
logic for different languages. This brings us back to either having different JavaScript files to
maintain or having large if-else chains.

Fortunately, most localization libraries provide the option to do parameterized substitution.
Parameterized substitution is similar to prepared statements for SQL^[mysql-prepared-statements].
Instead of doing string concatenation to create the final string, we instead have a template string
with indicators of where data should go. When we then need the final string, we specify the template
we want to use and the data we want to substitute. The localization library will then handle
substituting the data for us and give us the final string. In code, it would look like the following:

```javascript
// main-intl.js
const intl = getTranslationEngine(); // this is a placeholder for getting an instance whichever internationalization library you use
const response = await fetch('/api/current_user');
const currentUser = await response.json();
const content = document.getElementById('content');
if (currentUser.isAdmin) {
    content.innerHTML = intl.localize('user.greet.admin', {fullName: currentUser.fullName});
}
else {
    content.innerHTML = intl.localize('user.greet.nonAdmin', {fullName: currentUser.fullName});
}
```

Our English JSON file will now look like the following:

```json
{
  "user.greet.admin": "Hello {fullName}, you're an admin!",
  "user.greet.nonAdmin": "Hello {fullName}!"
}
```

Here you can see the parameter "fullName". "fullName" is just a template placeholder. Whatever value
we give "fullName" in the code will be inserted into this spot. Since the template is fully separated
from our code, translators are free to move those parameter values around however they need to for
the correct word order.

With parameter substitution, we can now handle the Chinese case properly where the name comes first.
Our Chinese text file will look like the following:

```json
{
  "user.greet.admin": "您好{fullName}, 您是管理者！",
  "user.greet.nonAdmin": "您好{fullName}！"
}
```

One thing you may have noticed is that we are no longer concatenating text based on if the user is
an admin or not. That's intentional! If we want to be able to adapt to any word order for any
language, we cannot concatenate anything. Instead of concatenating substrings we know about in code,
we do that concatenation ahead of time in the translation files. We then replace the text concatenation
with id selections in code. By avoiding concatenation, we can guarantee that translators are able
to give us the correct word order.

## Languages and Grammatical Gender

Many languages have a grammar rule called "grammatical gender." These languages include many European
languages, such as Spanish, French, German and Russian. With grammatical gender, different
words will change to match the "gender" of a noun. The gender may correspond to the gender of
a person, or it may correspond to the gender assigned to word by the language. For example, consider
the phrase "Hello {fullName}, you're an admin!". In Spanish, there are two translations depending on
the gender of the subject (in this case, it's the gender of the current user). The first translation
is the masculine form, "¡Hola {fullName}, usted esta un administrador!". The second translation is
the feminine form, "¡Hola {fullName}, usted esta una administradora!". Notice how in the second
translation it ends with "una administradora" instead of "un administrador". Two words change
depending on the gender of the current user.

In languages with grammatical gender, it's not just about getting the
user's pronouns correct (he, she, they, etc.), it's also about getting the entire sentence's word
choice correct. Getting it wrong could mean that the whole sentence is using the wrong grammar
and pronouns. There are three ways main ways to avoid using the wrong gender for nouns: limit what gets
substituted/describes users (or user-provided nouns), do pre-substitution for nouns known ahead of time,
or include grammatical gender information in substituted data.

The first option is the easiest. Instead of saying "Hello {fullName}, you're an admin!", just
display the user's full name. Don't include details about the user unless absolutely needed.
In most cases, the user will generally know if they're an admin since they will see admin-only
options in the application's menu. If the user really needs to check, have a separate place which
describes the assigned roles and permissions. When there are grammatical gender issues in text, it's
often from combining substitution with detailed information. Always ask yourself "Can I rewrite this
sentence so that there is no data substitution? If not, can I remove information from the sentence
and still have the site understandable? Is there another way to convey this information without using
text?"

If data substitution is still needed, then try to use pre-substitution. With pre-substitution,
the goal is to eliminate grammatical gender issues by substituting all the words an application knows about before text is handed to translators. Substitutions can either be done
manually by developers, or they can be done as part of an automated process (such as the build
pipeline). For example, consider a game that announces whenever the player sits on an object.
The game has five objects that the player can sit on, which are a "chair", a "horse", a "bench",
a "bucket", and a "cactus". These nouns are going to be substituted in the phrase "{playerName}
sat on a {satObject}". Instead of translating the words "chair", "horse", "bench", etc. separately
and then substituting the translations, generate the strings "{playerName} sat on a chair",
"{playerName} sat on a horse", "{playerName} sat on a bench", etc. Now, when translators are making
translations they can adapt the sentence to the grammatical gender of what the player sat on.

The third option is the most difficult, and it will impact much more of an application than simply
"substituting text". Whenever the above options don't work, in order to work with grammatical
gender an application need to collect the gender of entities that text refers to. Generally,
this happens when text refers to users, such as with the sentence "Hello {fullName}, you're
an administrator". For the translation to work properly, the application must collect the gender
for the user and then provide both the name and the user's gender to the substitution. Some libraries,
such as ICU Intl^[icu-message-format], allow for the selection to happen in the data file which
allows the code to be cleaner. In these cases, the code would look like the following:

```javascript
// main-intl.js
const intl = getTranslationEngine(); // this is a placeholder for getting an instance whichever internationalization library you use
const response = await fetch('/api/current_user');
const currentUser = await response.json();
const content = document.getElementById('content');
if (currentUser.isAdmin) {
    content.innerHTML = intl.localize('user.greet.admin', {fullName: currentUser.fullName});
}
else {
    content.innerHTML = intl.localize('user.greet.nonAdmin', {fullName: currentUser.fullName});
}
```

Our Spanish JSON file will now look like the following:

```json
{
  "user.greet.admin": "{gender, select, male {¡Hola {fullName}, usted esta un administrador!} female {¡Hola {fullName}, usted esta una administradora!}}",
  "user.greet.nonAdmin": "¡Hola {fullName}!"
}
```

However, as soon as the application starts collecting user gender information, the development choices
for the application become intertwined with politics. Additionally, languages can be hundreds or
thousands of years old, and the most popular languages have existed well before contemporary
political movements. Consequently, some of them may have grammatical limitations that can
conflict with modern trends, such as including non-binary gender options for users^[uc-davis-gender]^[u-of-pittsburgh-gender]^[us-passports-gender].
When there is a disparity between modern trends, software, and languages then a decision will need
to be made at some level.

For example, Spanish^[spanish-grammatical-gender] only supports two "genders" in their grammar:
masculine and feminine. In these cases, when a user selects a non-binary gender, someone has to decide
which grammatical gender the non-binary gender gets mapped to. If developers don't want to decide,
then they will often add an "other" gender to their gender selection templates as follows:

```json
{
  "user.greet.admin": "{gender, select, male {¡Hola {fullName}, usted esta un administrador!} female {¡Hola {fullName}, usted esta una administradora!} other  {¡Hola {fullName}, usted esta un administrador!}}",
  "user.greet.nonAdmin": "¡Hola {fullName}!"
}
```

While an "other" option in the JSON files may seem like a good solution on the surface, what it does
is move the decision from the developers to the translators. In the above, the "other" field is
still given a gender. In this case, the translators chose the "masculine" gender for anyone who chose
a non-binary gender. Translators could just as easily have chosen a "feminine" gender, or they could
make the subject plural instead of keeping it singular.

If the translators are internal to the company, then the decision would most likely reflect the
company's values and political stance. If the translators are external to the company, then their
decision may not reflect the company's values. In some cases, if job of translating is given to a
consulting firm then some text may choose the "masculine" gender and other text may use the
"feminine" gender for non-binary genders. The gender mismatches can happen depending on which person
translated which piece of text. If a company wants to guarantee a consistent portrayal of their
values and stances, then a company should consider making the decision of which gender to
use prior to sending the text off to translators for translation. The decision could be embedded in
code, or it could be documented in guidelines and policies for translators.

## Languages and Pluralization

Another common issue with localization is pluralization of words, especially when combined with a
number that is not known ahead of time (such as "*x* dog/dogs"). Languages do pluralization in
different ways, including having many plural forms or no plural forms. To simplify the discussion of
plurals, we'll only look at pluralization with numbers, and we'll ignore negative values
(anything less than 0).

English has a singular form for the number 1 ("one word") and plural forms for anything else ("0 words",
"2 words", "100 words")^[english-plurals]. French uses a singular form for 0 and 1 ("0 mot", "1 mot")
and plural for anything else ("2 mots", "100 mots")^[french-plurals]. Russian has three cases 
depending on the last digit of the number ("1 рубль", "2 рубля", "5 рублей")^[russian-plurals].
Chinese only has plurals for people which is done by adding "们"， nothing else has plurals ("1 熊",
"2 熊", "3 熊").

What this means for pluralization is that applications need to not couple code with pluralized
forms. Instead, code will need to lean on the pluralization features of libraries (including custom
libraries and wrappers) to handle selecting the correct form based the number. For example, if we wanted
to show how many bears we found, our code might look like the following:

```javascript
const bears = Math.floor(Math.random() * 100);
const intl = getLocalizationEngine();
console.log(intl.localize('count.bears', {bears: bears}));
```

Our JSON might look like the following:

```json
{
  "count.bears": "{bears, plural, offset:1 =0 {No bears found.} =1 {Found # bear.} other {Found # bears.}}"
}
```

To get translation done properly, we would either need to have translators manually fill in all
needed forms for our engine (which may not be possible if outsourcing), or we would need a
multiplexing process to generate placeholders for all the plural forms that
translators could then fill in, similar to what Etsy does^[etsy-plurals].

A final alternative would be to avoid combining pluralization and substitution. If an application
only allows a finite amount of numeric values, then it could have translation text for every
possible numeric value and avoid the substitution. For example, consider the following code:

```javascript
const bears = Math.floor(Math.random() * 4);
const intl = getLocalizationEngine();
console.log(intl.localize('count.bears', {bears: bears}));
```

With this code, there can be only 0, 1, 2 or 3 bears found. Instead of substituting the number of
bears and have to deal with pluralization in-code, we could create four strings and select between
them at runtime, as follows:

```javascript
const bears = Math.floor(Math.random() * 4);
const intl = getLocalizationEngine();

let text = '';
if (bears === 0) {
    text = intl.localize('count.bears.0')
}
else if (bears === 1) {
    text = intl.localize('count.bears.1');
}
else if (bears === 2) {
    text = intl.localize('count.bears.2');
}
else if (bears === 3) {
    text = intl.localize('count.bears.3');
}
console.log(text);
```

Our JSON would look something like:

```json
{
  "count.bears.0": "No bears found.",
  "count.bears.1": "1 bear found.",
  "count.bears.2": "2 bears found.",
  "count.bears.3": "3 bears found."
}
```

The translation file could then be given to translators who will be able to accurately translate all
possible texts.

One final thing to keep in mind with pluralization is it can be an exponential problem. The more plurals
and numbers in a sentence, the exponentially greater number of translations will be needed. The
more translations needed, the more memory and compute will be spent loading them, and the more money
will be spent translating them.

Pluralization becomes exponential when there are multiple plural terms in the same translation
unit. For example, consider the sentence "Your report finished with 1 critical alert, 3 errors,
and 2 warnings". To have proper localization, the software will need to have a translation for
every plural form of "critical alert". And for every plural form of "critical alert" it will need
every plural form of "errors". And for every plural form of "critical alert" and every plural form
of "errors" it will need every plural form of "warnings". This means that for *n* plural forms
in a language, we will need *n* \* *n* \* *n* plural forms or *n*^3. For English, this means we need 8
versions of the same sentence, and for Russian it means we need 27 versions of the same sentence. 
Or, to generalize it, for any sentence we will need *n*^*p* versions of that sentence where *n* is
the number of plural forms in the language and *p* is the number of plurals in the sentence. 

Fortunately we can turn pluralization into a linear problem so long as there is only at most one
plural form in any given sentence, and each sentence is its own translation unit (or text unit).
If we rewrite our sentence to "Your report finished. It had 1 critical alert. It had 3 errors.
It had 2 warnings." then we can break it up into the following translation units:
 * "Your report finished."
 * "It had 1 critical alert."
 * "It had 3 errors."
 * "It had 2 warnings."

Now that our plurals are in separate translation units, the total number of sentences we need is 1
for "Your report finished.", *n* for "critical alert", *n* for "errors", and *n* for "warnings",
which becomes 1 + *n* + *n* + *n* or 1 + 3 \* *n*, which is much better than the *n*^3 from earlier.
For English, this means we need 7 sentences in total, and for Russian it means we need 10 sentences
in total.

## Localization and tooling

For the keen-eyed among you, you may have noticed that in my fixed examples, I was overly verbose.
In most of my examples, I could have saved lines of code by doing string concatenation to generate
the ids for localized text. For instance, instead of writing

```javascript
const bears = Math.floor(Math.random() * 4);
const intl = getLocalizationEngine();

let text = '';
if (bears === 0) {
    text = intl.localize('count.bears.0')
}
else if (bears === 1) {
    text = intl.localize('count.bears.1');
}
else if (bears === 2) {
    text = intl.localize('count.bears.2');
}
else if (bears === 3) {
    text = intl.localize('count.bears.3');
}
console.log(text);
```

I could have written

```javascript
const bears = Math.floor(Math.random() * 4);
const intl = getLocalizationEngine();
console.log(intl.localize('count.bears.' + bears));
```

I intentionally did not write the shorter version for one simple reason: tooling.

When localizing a product, it is important to have tooling to help catch typos in ids,
find which ids are no longer referenced in code, and catch other common mistakes. The goal for any
localization effort is to empower every developer on a product to incorporate localization into
their code. Since developers are human, they will need a way to catch simple mistakes, such as typos
in their ids. The harder it is for them to verify their localized code is correct, the less likely
they will be to maintain the localized code. Localization ids are almost always strings, so compilers
will not check that they are correct. This leaves it up to developers to figure out how to ensure
the code is correct.

When starting a localization effort, it may be tempting to limit our thinking to just "How do we
catch developers' mistakes before they go into production?"
For this question, the answer usually as simple as "code reviews and manual testing."
I've gone down this route. In practice, my code reviewing didn't take too long before I found a bug.
I was both very familiar with the code, and I had tooling to help me catch bugs during manual
testing. My review process was about 5-10 minutes to find the first bug in a PR, and about another 20-30
minutes to find all bugs in a PR.

One thing I noticed was that the PRs almost all had the same two or three type of bugs, with the most
common being a typo in a string id. Additionally, I noticed that most PRs went back and forth because
usually more typos were made trying to fix the initial typos. During this process, my question
shifted from "How do I catch developers' mistakes?" to "How do I empower developers to fix their own
mistakes?" After talking to other developers on my team, we came up with an idea to write tooling to
catch the common mistakes. So, I began to work on tooling to catch and report mistakes so that developers
fix any mistakes before they submitted their PR. Once the tooling was in the hands of
other developers, the error rate dropped from 9 out of 10 PRs having localization bugs
to 2 out of 10 PRs having localization bugs. With some more improvements, we got it down to less than
1 out of 10 PRs having localization bugs.

When developing localization tooling, the bare minimum it should do is:
 * Find IDs referenced in code which do not exist in the data files
 * Find IDs that exist in the data files which are not referenced in code

The first one is pretty obvious. If the code is using an ID that does not exist, then the application
will hit an error state. The error state could result in a crash, the id being shown, an error message
being displayed, or an empty UI element, none of which are a good user-experience. A tool that can
detect invalid ids in the code will be able to stop those mistakes before it reaches customers.
The easiest way to do that check is to make the code verbose and clear on when it's using a string
as an id (i.e. a parameter to the "localize" method) and when it's not. It's also easier to detect
and check fully-typed out strings than it is to detect strings with concatenation going on.

The second check is more about not wasting resources. Every piece of text in a translation file
consumes resources. Some of these resources are computer resources, such as the memory and time it
takes to load and store unused ids. However, the most notable resource is the bill paid for
translating the text. Whenever a piece of text gets sent to a translator, the translator gets paid
to do the translation - even if the code won't actually use the result. By being able to
detect what strings are unused by code, companies can save on translation costs. Also, being able
to remove cruft from data files will help keep them clean, lean, and easier to manipulate and work
with.

To develop both pieces of the minimal tooling, it's best to have practices that are verbose. It's
far easier to know the value of a string literal than it is to know the value (or potential values)
of a variable. For that reason, I choose to have if statements choose between calls to my localize
methods rather than have if statements choose between values to assign to a variable, as shown
below.


```javascript
const isTrue = await getTheAnswer();

// Can't assume every string is an ID or user facing; many of them will be your debug logs
const logMessage = isTrue ? 'The answer is true' : 'The answer is false';
getFileLogger().log.trace(logMessage);

// This is harder to write tooling which can pick out what's a localization id
// Would have to find the variable declaration and all of it's usages and figure out
// what the value is getting set to. That's a lot of work for every programming language
// that's getting tooling
let textId = isTrue ? 'true.id' : 'false.id';
// ... some code here which may or may not change textId ...
const text = intl.localize(textId);

// My preference: This is easier to write tooling which can pick out what's a localization id
// No need to parse the whole file or figure out what variables are doing.
// Can sometimes be done with a regex that could be used across several programming languages
const text = isTrue ? intl.localize('true.id') : intl.localize('false.id');
```

The bare minimum tooling will help catch most of the noticeable errors. Other errors will creep
through, so tooling can be enhanced to catch more mistakes. Some companies and libraries will
have different catalogs of "mistakes", so some of what I consider a mistake may not be a considered
a mistake everywhere. Below is my list of other mistakes I look for in localization changes:

* Missing substitution data for parameters in template strings
* Passing in too much parameter data (especially sensitive data)
* Overusing the same text id
* Overusing parameter substitution in text

There are other mistakes that could be checked, such as making sure any typed substitution parameters
match up with the type of the substituted data. Sometimes these things are very easy to check with
code, other times it's much easier to check by running the code or with a code review. What tools each
company develops will depend on their needs and the resources they have available for tooling.

## Conclusion

Localization is often harder than people expect because languages usually don't work the same way someone's
native language works. Consequently, it's very easy for developers to fall into anti-patterns that
need to be reworked simply because they made assumptions based on how their native language works.
Sometimes it's possible to tweak the code, other times it's much cheaper to change the text.
In this post, I just covered the tip of the localization iceberg. In future
posts, I want to discuss some more challenges further, such as numbers, dates and times, phone numbers,
names, text and UI direction, and more.

@References
* user-language
  | type: web
  | page-title: Internet World Users by Language
  | website-title: Internet World Stats
  | link: https://www.internetworldstats.com/stats7.htm
  | date-accessed: 2022-09-22
* linguistics-number-of-languages
 | type: web
 | page-title: How many languages are there in the world?
 | website-title: Linguistic Society of America
 | link: https://www.linguisticsociety.org/content/how-many-languages-are-there-world
 | date-accessed: 2022-09-22
* mysql-prepared-statements
 | type: web
 | page-title: MySQL Prepared Statement
 | website-title: MySQL Tutorial
 | link: https://www.mysqltutorial.org/mysql-prepared-statement.aspx
 | date-accessed: 2022-09-26
* icu-message-format
  | type: web
  | page-title: Formatting Messages
  | website-title: ICU Documentation
  | link: https://unicode-org.github.io/icu/userguide/format_parse/messages/
  | date-accessed: 2022-09-26
* uc-davis-gender
 | type: web
 | page-title: Gender Identity
 | website-title: UC Davis Graduate Studies
 | link: https://grad.ucdavis.edu/gender-identity-and-sexual-orientation-questions
 | date-accessed: 2022-09-26
* u-of-pittsburgh-gender
 | type: web
 | page-title: New Gender Identity Options Available on University Employment Application
 | website-title: University of Pittsburgh
 | link: https://www.hr.pitt.edu/news/new-gender-identity-options-available-university-employment-application
 | date-accessed: 2022-09-26
* us-passports-gender
 | type: web
 | page-title: Selecting your Gender Marker
 | website-title: Travel.State.Gov: U.S. Department of State - BUREAU of CONSULAR AFFAIRS
 | link: https://travel.state.gov/content/travel/en/passports/need-passport/selecting-your-gender-marker.html
 | date-accessed: 2022-09-26
* spanish-grammatical-gender
 | type: web
 | page-title: Spanish Grammar: Figuring out Grammatical Gender
 | website-title: +Babbel Magazine
 | link: https://travel.state.gov/content/travel/en/passports/need-passport/selecting-your-gender-marker.html
 | date-accessed: 2022-09-26
* english-plurals
 | type: web
 | page-title: English plurals
 | website-title: Wikipedia
 | link: https://en.wikipedia.org/wiki/English_plurals
 | date-accessed: 2022-09-26
* french-plurals
 | type: web
 | page-title: Plural in French (nouns)
 | website-title: Language easy.org
 | link: https://language-easy.org/french/grammar/nouns/plural/
 | date-accessed: 2022-09-26
* russian-plurals
 | type: web
 | page-title: Russian Plurals - Russian Language Lesson 11
 | website-title: Russian Lessons.net
 | link: https://www.russianlessons.net/lessons/lesson11_main.php
 | date-accessed: 2022-09-26
* etsy-plurals
 | type: web
 | page-title: Plurals at Etsy
 | website-title: Etsy Code as Craft
 | link: https://www.etsy.com/codeascraft/plurals-at-etsy
 | date-accessed: 2022-09-26