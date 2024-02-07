# matthewtolman.com

This is the code behind my personal website. For this code, I have a custom static site generator which includes custom HTML/JavaScript output generation, asset packing, XML config, and a custom Markdown-like parser.

For the output, I focused on minimizing JavaScript, making 3rd party JavaScript libraries optional, and storing any settings/state in LocalStorage on the browser. The site operates normally even with JavaScript disabled in the browser. Turning on JavaScript allows the user to change reading settings, such as font size, font family, spacing, etc. These settings are saved to LocalStorage and are never sent to a server. This means that the settings are device local rather than user specific (since users can have multiple devices). However, it allows for both increased privacy, removes the need for credential management, and lowers server costs, so it's a fair trade off. 3rd party JavaScript may also be turned on optionally. The 3rd party JavaScript is primarily for interactive and accessible LaTex rendering rather than using an SVG. These 3rd party libraries are written by one 3rd party and hosted by another, so there may be some security and privacy concerns for some. As such, they are turned off by default but may be turned on as a setting. It also reduces initial load times for first time visits to the site as the amount of HTTP requests made is limited.

For font families, I use the native font families used in [Reboot from Bootstrap](https://getbootstrap.com/docs/4.0/content/reboot/#native-font-stack). Most options available also use native font stacks. However, there is one font family which uses a WebFont that must be downloaded called Open Dyslexia. Open Dyslexia is opt-in and only downloaded when a user chooses it as their active font family.

## Markdown Parsing

My Markdown-like parser is able to handle basic markdown syntax which I user personally. It includes headings, tables, links, blockquotes, and code samples. It also has extensions for table of contents, bibliographies, and links to articles by ID. Here's an example:


````markdown
Making a table of contents is easy.

[toc]

# H1 support

Paragraphs are separated by double newlines.
Single newlines are still part of the same paragraph.

This would be a new paragraph.

## Subheadings work too

Code can be embedded as well. Embedded code will automatically be truncated with an "expand" button if it has more than 20 lines. A langauge does need to be specified after the triple backtick for it to work properly.

```css
body {
    color: #444
}
```

Inlined code can be done with `single backticks`.

Tables also work.

| Column 1 | Column 2 |
|----------|----------|
| Cell 1, 1 | Cell 1, 2 |

Links with names work as expected [example](example.com). All links need to be named

Lists are supported with a leading dash
- Item 1
- Item 2

Lists can also use leading asterisks:
* Item 1
* Item 2

Code can be formatted with asterisks for *italic* and **bold**.

Article links can be done with double square brackets. \[\[article:article-id-here]]. This will link to the article, regardless of where the final output is, and it will use the article title for the link text. This allows having "permalinks" regardless of how a website is reorganized later. It also allows seeing if an article is being linked to prior to deletion.

HTML support is not available. Instead, we use a separate notation to indicate raw HTML elements. By not supporting raw HTML the parser is able to stay simpler, and we can also use < and > directly in our article without having to escape them.  The notation is as follows:

```text
ELEM = TAG_START TAG_NAME (ATTR_SEP ATTR)* (ATTR_SEP TEXT ATTR_SEP CLOSE_TAG_NAME)? TAG_END
TAG_START = ~
TAG_NAME = \w+
ATTR_SEP = ::
ATTR = ATTR_NAME=ATTR_VALUE
ATTR_NAME = (\w | -)+
ATTR_VALUE = (:?[^:])*?)+
TEXT = .*
CLOSE_TAG_NAME = TAG_START SLASH TAG_NAME
SLASH = /
TAG_END = :;
```

For example, to embed an image we would do `~img::src=/example.png::alt=Some alt text here:;`.

For creating a span with a specific style, we'd do `~span::style=color:blue::My span text here::~/span:;`

For a line break, we'd do `~br:;`

This allows us to have HTML elements, but in a way that's easier to parse.

Text escaping is also possible using back slashes. So we can write \~ to escape a ~.

Another extension is embedded LaTeX for math equations. We can embed multiline LaTeX using `$$` as follows:

$$
\frac{r\left(P_V\right)}{1-\left(1 + r\right)^{-n}}
$$

We can also embed inlined LaTeX with `\(` and `\)` such as \(P_V\).

Quote blocks are available by prefixing text with `>`. Note that only a `>` at the start of a line will trigger a quote block. For example, I can write > in the middle of the line and not get a block quote.

> But this is a block quote

Bibliographic references are done with `^[ref-id]`. For instance, I can `reference something^[example-ref]` in my text.
Once a reference is added, a Bibliography must be defined as well. This is done by defining a references section with `@References`.
Each reference is then defined by prefixing it with as follows:

```text
REFERENCE = REF_START REF_ID (REF_ATTR REF_ATTR_SEP REF_VALUE)+
REF_ID = (?:\*\s*(?:[\w\-]+)
REF_ATTR = (?:[\w\-]+)
REF_ATTR_SEP = :
REF_VALUE = [^\n]+
```

The generator will then create citations in the IEEE format. Currently, only web references are supported, though I'll add more as needed. A reference usually looks like the following:

```text
@References
* mdn-iterators
  | type: web
  | page-title: Iterators and generators
  | website-title: Mozilla Developer Network
  | link: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Iterators_and_Generators
  | date-accessed: 2022-09-20
```

The bibliography will automatically be added to the table of contents, and references will link to the bibliography automatically. Since the IEEE format is used, refrence links in text will be of the format `[1]`, `[2]`, etc.

````

The markdown-like parser is designed to (at least for now) be parseable by a JavaScript regular expression. The exclusion of traditional HTML tags helps make this possible, as well as restrictions on what is allowed. For content that can contain content (e.g. table cells), the inner content does need to get ran through the parser again, which can lead to recursive parsing. However, this hasn't been an issue and I've been able to rebuild the entire site quickly with a watch.

## XML Configuration

This project was originally intended to control the build system for multiple websites. However, the way I use it with Cloudflare makes it so that I can only practically do a single website. However, this background is important for the XML configuration to make sense. 

> At some point that will probably change by making the generator be an out-of-process build system and I just commit output files rather than source files.

For the configuration, all websites are subdirectories of the `data` directory. Each website directory has a `config.xml` file which describes that website. The XML file describes the following:
* Name of the website
* Description of the website
* Copyright holder
* Base URL (often used for links and images)
* Page icon
* Folder for images
* Categories for RSS feeds
* List of articles
  * Includes article template and list template
* Authors of the website
* List of assets

The XML structure is as follows:
```xml
<?xml version="1.0" encoding="utf-8"?>
<!-- blog describes the website -->
<blog
        title="Name of the website"
        description="Description of the website" 
        copyright-holder="What to show as the copyright owner"
        base-url="Base URL of the website"
        icon="Image to use for the home icon"
        img-folder="relative path to image folder to bundle">
    <!-- This is meant for RSS feeds & readers to categorize the blog -->
    <categories>
        <category>Programming</category>
        <category>Web Development</category>
    </categories>
  <!-- List of articles and their information -->
  <articles
          articleTemplate="Relative path to mustache template file for a single article"
          listTemplate="Relative path to mustache template file for list of all articles">
    <!-- Article details. One entry is required per article -->
    <!-- The order articles are listed here will determine the order they are listed on the site   -->
    <article
            published="Yes or No; Yes means list it, No means don't. Unlisted articles will still have a page generated, but won't show up in the article list"
            title="The title of the article"
            author="Object ID for the author (e.g. auth:jdoe)"
            time="ISO 8601 format for the time published"
            id="Object Id for the article (e.g. article:my-article)"
            file="File for the article, relative to the articles directory (e.g. articles/my-article.md would be the value my-article.md)">
      <description>
        Description of the article to show in the article list. This will also be used in the RSS feed to describe the article. Note: This is not Markdown or Markdown like syntax. It's just a blob of text that ignores formatting (though I might add line breaks in the future).
      </description>
    </article>
  </articles>
  <!-- List of authors who publish to the blog -->
  <!-- This determines how authors are listed on the blog posts they write -->
  <authors>
    <!--  Information about an author  -->
    <author
            id="ObjectID of the author (e.g. auth:jdoe)"
            name="Name of the author">
      <bio>A short bio description. Currently not used.</bio>
      <!-- In the future I might add more fields (e.g. profile picture). For now though, this covers what I need -->
    </author>
  </authors>
  <!-- Describes assets that need to be processed/packaged for the site to work -->
  <assets>
    <!--  Stylesheet assets include CSS and SASS (e.g. scss)
      They are converted to CSS (if needed) and minified
       prior to being put in the output directory. 
      The file name and relative paths remain the same, but the
       extension always becomes css
       -->
    <stylesheet type="scss" src="stylesheets/main.scss"/>
    
    <!-- Script assets are for JavaScript files that get minified 
      The file name and relative paths remain the same, but the
       extension always becomes js -->
    <script type="js" src="script/mathjax-fallback.js"/>
    
    <!-- copy assets have no processing done to them.
        They are just copied to the output directory with the same relative paths.
        These are often used for favicons, robots.txt, and site manifests -->
    <copy src="favicon.ico"/>
    <copy src="site.webmanifest"/>
    <copy src="robots.txt"/>
  </assets>
  <!-- portfolio is currently unused -->
  <portfolio></portfolio>
</blog>
```

### Non-configurable configuration

There are a few things that rely on convention or hard coding over configuration. At some point I'll get these fixed, but for now it's important to note them.
* `settings-templ.mustache` is the hard-coded path for the website settings page template file. This does need to get put as a site configuration property at some point
* Article files are always in the `articles/` directory. Again, this should probably get updated to be a property
* Custom fonts are always in the `fonts/` directory, which should also get updated to be a property
* Assets should have overrides for the final asset name (currently they share the same basename but may change extensions)
* Settings are all hardcoded in `src/generator.js`. Ideally, the settings available for a website should be configurable (e.g. should be able to change the XMl to add/remove 3rd-party JavaScript, add new font families, etc)
* RSS settings (e.g. name, location, format) are all hardcoded in `src/generator.js`
* SVGs for RSS and Settings icons (e.g. for mobile) are not configurable
* Highlight JS plugins for new languages are hardcoded into `src/generator.js`
* No option exists to embed assets into the output. This may be useful for CSS/JavaScript at some point. I haven't worried about it since the site loads really quickly (there isn't a lot of code right now)
* No option for how to process images (e.g. using `<picture>` with `srcset` to load pre-scaled images, converting to different formats, etc). I haven't worried about images too much since most images on the site are SVGs which load quickly, and the images which aren't SVGs tend to be 60KB or less, so they load pretty quickly too.
* No options for preprocessing dot files into SVG files. Currently I've been preprocessing dot files manually, but at some point I'd like to make that an automated process

## Note on the "Security Vulnerability"

Currently, when you run `npm install` it says there is a security vulnerability. The vulnerability is related to the XML parser being susceptible to XXE Injection attacks. Of course, this requires the XML parser to be feed XML that contains XXE in the first place.

Currently, the only XML fed to the XML parser is XML which I have written. And I don't like XXE, so I don't use XXE. Which means that I'm not going to run into XXE Injection because there is no XXE in the first place.

If this library was downloading or parsing untrusted XML, then yes the vulnerability would need to get fixed (i.e. don't use this as a backend for a website builder where people can upload XML files). But since that's not what's happening, it's a non-issue.

I haven't fixed it to make NPM happy because the "fixed" version is completely incompatible, and it'd require quite a decent rewrite. At that point, there's a whole list of rewrites I'd like to do, and languages I'd like to try the rewrite in, so I'd probably scrap most of the current parser/generator and begin again. I don't feel like doing that right now, which is why I've just left it alone and ignored it.

## License

I'm using GPL v3 for this project. There is no offering for a "commercial" license. There's plenty of site generators out there with more permissive licenses, so feel free to use something else. Here's a list someone else put together: https://jamstack.org/generators/.

## Contributions

No contributions are accepted at this time. Once the generator is separate from my blog's content I'll start accepting contributions in the generator repository.
