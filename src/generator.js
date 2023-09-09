const { parseBlog } = require('./parser');
const path = require('path');
const { render } = require('mustache');
const { copy, writeFile } = require('fs-extra');
const { htmlDecode, htmlEncode } = require('js-htmlencode');
const { format } = require('date-fns');
const hljs = require('highlight.js');
const fs = require('fs');
const { ArticleTokenType } = require('./types');

const dateFormat = "yyyy-MM-dd'T'HH:mm:ssXX";
const mkdirp = require('mkdirp');

let mathJaxSvgContext = null
let mathJaxCHtmlContext = null

function setMathJaxSvgContext(context) {
  mathJaxSvgContext = context
}

function setMathJaxCHtmlContext(context) {
  mathJaxCHtmlContext = context
}

function generateBlogHtml(xmlFile, directory) {
  return parseBlog(xmlFile, directory)
    .then((blog) => [blog, []])
    .then(generateArticleHtml)
    .then(generateArticlesHtml)
    .then(generateSettingsHtml)
    .then(generateRssFeed)
    .then(writeFiles)
    .finally(() => {
      console.log('Finished');
    });
}

const globalScript = `<script>
window.addEventListener('load', function () {
    loadSettings()
})

function textFontToClass(font) {
    switch(font) {
        case 'open-dyslexia':
            return 'open-dyslexic-text-font'
        case 'arial':
            return 'arial-text-font'
        case 'comic-sans':
            return 'comic-sans-text-font'
        default:
            return 'default-text-font'
    }
}

function codeFontToClass(font) {
    switch(font) {
        case 'open-dyslexia-mono':
            return 'open-dyslexic-code-font'
        case 'courier-new':
            return 'courier-new-code-font'
        default:
            return 'default-code-font'
    }
}

function loadSettings() {
    if (window.localStorage) {
        while(document.body.classList.length) {
            document.body.classList.remove(document.body.classList[0])
        }

        document.body.classList.add(textFontToClass(localStorage.getItem('font') || ''))
        document.body.classList.add(codeFontToClass(localStorage.getItem('codeFont') || ''))

        localStorage.getItem('increaseLetterSpace') === 'true' && document.body.classList.add('a11y-increased-letter-spacing')
        localStorage.getItem('increaseWordSpace') === 'true' && document.body.classList.add('a11y-increased-word-spacing')
        localStorage.getItem('increaseLineHeight') === 'true' && document.body.classList.add('a11y-increased-line-height')
        localStorage.getItem('increaseParagraphSpace') === 'true' && document.body.classList.add('a11y-increased-paragraph-spacing')
        
        if (window.CSS && window.CSS.supports('color', 'var(--fake-var)')) {
            document.documentElement.style.setProperty('--font-size', localStorage.getItem('fontSize') + '%')
        }
    
        switch(localStorage.getItem('theme')) {
            case 'dark':
                document.body.classList.add('dark-theme')
                break
            case 'light':
                document.body.classList.add('light-theme')
                break
        } 
    }
}
</script>`;

const settingsIcon = fs.readFileSync(path.join(__dirname, 'settings.svg'));

function blogMustacheBase(blog) {
  return {
    global_assets: blog.assets.stylesheets
      .map((s) => `<link rel="stylesheet" href="/${s.fileName}" />`)
      .concat([globalScript])
      .join(''),
    navigation: `<div class="large-nav">
<span class="site-icon"><a class="blog-icon-link" href="/" aria-labelledby="blog-icon-l">${blog.icon}<span id="blog-icon-l" class="a11y-sr-only-nf">${blog.blogTitle}, Home</span></a></span><ul><li><a href="/">Blog</a></li><li class="settings-link-li"><a href="/settings.html">Settings</a></li></ul>
</div><div class="small-nav">
<span class="site-icon"><a class="blog-icon-link" href="/" aria-labelledby="blog-icon-s">${blog.icon}<span id="blog-icon-s" class="a11y-sr-only-nf">${blog.blogTitle}</span></a></span><ul><li class="settings-link-li"><a class="settings-link" href="/settings.html" aria-labelledby="settings-label">
<span class="a11y-sr-only-nf">Settings</span>
${settingsIcon}
</a></li></ul>
</div><noscript><style>.settings-link-li{display:none;}</style></noscript>`,
    copyright: `${blog.copyrightHolder}, ${new Date().getFullYear()}`,
    privacyStatement: `Privacy Policy: We don't set cookies, we don't collect data. We are hosted by Cloudflare, so check their policies to see what they collect.`,
  };
}

function flattenTokens(tokens) {
  return tokens.flatMap((t) => (typeof t === 'string' ? t : [t, ...flattenTokens(t.data)]));
}

function headerId(header, blog, references = {}, sections = []) {
  const content = contentToHtml(header.data, blog, references, sections).join('');
  return content.replace(/[^a-zA-Z0-9_]/g, '--').toLowerCase();
}

function generateTableCode(head, body) {
  let res = '<table>'

  res += "<thead><tr>"
  for (const headCell of head) {
    res += "<th>" + headCell.replace(/\\n/g, "<br/>") + "</th>";
  }
  res += "</tr></thead>"

  res += "<tbody>"
  for (const row of body) {
    res += "<tr>"
    for (const cell of row) {
      res += `<td>${cell}</td>`
    }
    res += "</tr>"
  }
  res += "</tbody>"

  res += '</table>'
  return res
}

function generateInlineCode(lang, code) {
  // We do an htmlDecode since the incoming content is htmlEncoded and hljs will do an additional htmlEncode
  return `<span class="hljs language-${htmlEncode(lang)}">${
      hljs.highlight(htmlDecode(code), { language: lang }).value
  }</span>`;
  // return
}

function generateCodeBlock(lang, code) {
  // We do an htmlDecode since the incoming content is htmlEncoded and hljs will do an additional htmlEncode
  return `<pre><code class="hljs language-${htmlEncode(lang)}">${
    hljs.highlight(htmlDecode(code), { language: lang }).value
  }</code></pre>`;
  // return
}



function contentToHtml(tokens, blog, references = {}, sections = []) {
  const res = [];

  function processList(token, tokens) {
    const elems = [`<li>${contentToHtml(token.data, blog, references, sections).join('')}</li>`];
    let j = 0;
    for (; j < tokens.length; ++j) {
      const nextToken = tokens[j];
      if (typeof nextToken === 'string') {
        break;
      } else if (nextToken.type !== ArticleTokenType.LIST) {
        break;
      } else if (nextToken.attrs.leadingWhitespace.length < token.attrs.leadingWhitespace.length) {
        break;
      } else if (nextToken.attrs.leadingWhitespace.length > token.attrs.leadingWhitespace.length) {
        const [count, child] = processList(nextToken, tokens.slice(j + 1));
        j += count;
        elems.push(child);
      } else if (token.attrs.ordered !== nextToken.attrs.ordered) {
        break;
      } else {
        elems.push(
          `<li>${contentToHtml(nextToken.data, blog, references, sections).join('')}</li>`
        );
      }
    }
    const tag = token.attrs.ordered ? 'ol' : 'ul';
    return [j, `<${tag}>${elems.join('')}</${tag}>`];
  }

  for (let i = 0; i < tokens.length; ++i) {
    const token = tokens[i];
    if (typeof token === 'string') {
      res.push(htmlEncode(token));
    } else {
      const content = () => contentToHtml(token.data, blog, references, sections).join('');
      switch (token.type) {
        case ArticleTokenType.BLOCK_QUOTE:
          res.push(`<blockquote>` + content() + `</blockquote>`);
          break;
        case ArticleTokenType.BOLD_ITALIC:
          res.push(`<strong><em>${content()}</em></strong>`);
          break;
        case ArticleTokenType.BOLD:
          res.push(`<strong>${content()}</strong>`);
          break;
        case ArticleTokenType.EMPHASIS:
        // fallthrough
        case ArticleTokenType.ITALIC:
          res.push(`<em>${content()}</em>`);
          break;
        case ArticleTokenType.PARAGRAPH_BREAK:
        // fallthrough
        case ArticleTokenType.PARAGRAPH:
          res.push(`<p>${content()}</p>`);
          break;
        case ArticleTokenType.HEADER:
          res.push(
            `<a class="anchor"></a><h${
              token.attrs.level || 1
            } tabindex="-1" id="__header--${headerId(
              token,
              blog,
              references,
              sections
            )}">${content()}</h${token.attrs.level || 1}>`
          );
          break;
        case ArticleTokenType.LIST: {
          const [count, str] = processList(token, tokens.slice(i + 1));
          res.push(str);
          i += count;
          break;
        }
        case ArticleTokenType.LINK:
          res.push(`<a href="${htmlEncode(token.attrs.ref)}" target="_blank">${content()}</a>`);
          break;
        case ArticleTokenType.ELEM: {
          const attrs = Object.keys(token.attrs)
            .filter((t) => t != 'tag' && t)
            .map((key) => `${key}="${htmlEncode(token.attrs[key])}"`)
            .join(' ');
          if (token.data.length == 0) {
              res.push(`<${token.attrs.tag} ${attrs} />`);
          }
          else {
              res.push(`<${token.attrs.tag} ${attrs}>${token.data.join('\n')}</${token.attrs.tag}>`);
          }
          break;
        }
        case ArticleTokenType.REF: {
          if (token.attrs.ref && references[token.attrs.ref]) {
            const linkHref = `__bilbiography__ref--${token.attrs.ref}`;
            const linkContent = references[token.attrs.ref].id;
            res.push(
              `<sup><a href="#${linkHref}">[<span class="a11y-sr-only-nf">Reference </span>${linkContent}]</a></sup>`
            );
          } else if (!token.attrs.ref) {
            console.warn('Found empty bibliography reference!');
          } else {
            console.warn(`Invalid bibliography reference ${token.attrs.ref}!`);
          }
          break;
        }
        case ArticleTokenType.TABLE_OF_CONTENTS: {
          const flattened = flattenTokens(tokens);
          const headers = flattened
            .filter((t) => typeof t !== 'string')
            .filter((t) => t.type === ArticleTokenType.HEADER);
          const headerTree = { level: 0, headers: [], name: '', id: '' };
          let curTreeNode = headerTree;
          const treeStack = [];
          for (let i = 0; i < headers.length; ) {
            const header = headers[i];
            if (header.attrs.level > curTreeNode.level) {
              treeStack.push(curTreeNode);
              const newTreeNode = {
                level: header.attrs.level,
                headers: [],
                name: contentToHtml(header.data, blog, references, sections).join(''),
                id: headerId(header, blog, references, sections),
              };
              curTreeNode.headers.push(newTreeNode);
              curTreeNode = newTreeNode;
              ++i;
            } else if (treeStack.length) {
              curTreeNode = treeStack.slice(-1)[0];
              treeStack.pop();
            } else {
              throw `Invalid header level ${header.attrs.level}`;
            }
          }

          if (Object.entries(references).length) {
            headerTree.headers.push({
              level: 1,
              headers: [],
              name: 'Bibliography',
              id: '__bibliography',
            });
          }

          const linkifyTree = function linkify(treeNode) {
            if (!treeNode.level) {
              return `<ol>${treeNode.headers.map(linkify).join('')}</ol>`;
            }
            const id =
              treeNode.id == '__bibliography' ? `#${treeNode.id}` : `#__header--${treeNode.id}`;
            if (treeNode.headers.length) {
              return `<li><a href="${id}">${treeNode.name}</a><ol>${treeNode.headers
                .map(linkify)
                .join('')}</ol></li>`;
            }
            return `<li><a href="${id}">${treeNode.name}</a></li>`;
          };
          res.push(`<section>
    <h2>Table of Contents</h2>
    ${linkifyTree(headerTree)}
</section>`);
          break;
        }
        case ArticleTokenType.OBJ_LINK: {
          const articles = blog.articles.filter((a) => a.id === token.attrs.ref);
          if (articles.length === 0) {
            throw `Invalid article id ${token.attrs.ref}`;
          }
          const article = articles[0];
          res.push(`<a href="${articleUri(article)}">${article.title}</a>`);
          break;
        }
        case ArticleTokenType.REFERENCE:
          // Intentional; REFERENCE is an internal-token type not meant for HTML production
          break;
        case ArticleTokenType.CODE_BLOCK:
          res.push(generateCodeBlock(token.attrs.lang, content()));
          break;
        case ArticleTokenType.INLINE_CODE:
          res.push(generateInlineCode(token.attrs.lang || 'cpp', content()));
          break;
        case ArticleTokenType.MATH:
          const text = token.attrs.inline ? `\\(${token.attrs.equation}\\)` : `$$${token.attrs.equation}$$`;
          const textClass = mathJaxSvgContext ? 'math-initial' : '';
          res.push(`<span class='${textClass}'>${text}</span>`);
          if (mathJaxSvgContext) {
            const htmlObj = mathJaxSvgContext.tex2svg(token.attrs.equation, {display: true})
            let html = MathJax.startup.adaptor.outerHTML(htmlObj);
            if (!token.attrs.inline) {
              html = `<div class="math-block math-fallback">${html}</div>`
            }
            else {
              html = `<span class="math-fallback">${html}</span></span>`
            }
            res.push(html)
          }
          break;
        case ArticleTokenType.TABLE:
          res.push(generateTableCode(token.attrs.head, token.attrs.body));
          break;
      }
    }
  }
  return res;
}

function articleToHtml(content, blog) {
  return (
    `<a tabindex="-1" class="anchor" id="__top"></a>` +
    contentToHtml(content.tokens, blog, content.references, content.sections).join('') +
    createBibliography(content.references) +
    `<a class="to-top" href="#__top">Back to Top</a>`
  );
}

function articleUri(article) {
  return '/' + `${article.id}.html`.split(':').join('/');
}

function generateArticleHtml([blog, files]) {
  console.log(`${blog.blogTitle}: Found ${blog.articles.length} Article(s)\n`);
  return [
    blog,
    files.concat(
      blog.articles.map((article) => {
        return {
          content: render(blog.articleTemplate, {
            ...blog,
            ...blogMustacheBase(blog),
            ...article,
            time: format(article.time, dateFormat),
            content: articleToHtml(article.content, blog),
          }),
          fileName: path.join(...`${article.id}.html`.split(':')),
        };
      })
    ),
  ];
}

function paginate(arr, pageSize) {
  const res = [];
  let cur = [];
  for (let i = 0; i < arr.length; ++i) {
    cur.push(arr[i]);
    if (cur.length >= pageSize) {
      res.push(cur);
      cur = [];
    }
  }
  if (cur.length) {
    res.push(cur);
  }
  return res;
}

const pageSize = 10;

function generateArticlesHtml([blog, files]) {
  const pages = paginate(
    blog.articles
      .sort((l, r) => (l.time < r.time ? 1 : l.time === r.time ? 0 : -1))
      .map((a) => ({ ...a, uri: articleUri(a), time: format(a.time, dateFormat) })),
    pageSize
  );
  return [
    blog,
    files.concat(
      pages.map((articlePage, index) => {
        return {
          content: render(blog.listTemplate, {
            ...blog,
            ...blogMustacheBase(blog),
            nextLink:
              index + 1 < pages.length
                ? `<a class="older-posts-link" href="page${index + 1}.html">Older Posts</a>`
                : '',
            prevLink: index
              ? `<a class="newer-posts-link" href="${
                  index == 1 ? 'index.html' : `page${index - 1}.html`
                }">Newer Posts</a>`
              : '<span></span>',
            articles: articlePage,
            articleJson: JSON.stringify(blog.articles),
          }),
          fileName: index ? `page${index}.html` : 'index.html',
        };
      })
    ),
  ];
}

function generateSettingsHtml([blog, files]) {
  return [
    blog,
    files.concat([
      {
        fileName: 'settings.html',
        content: render(blog.settingsTemplate, {
          ...blog,
          ...blogMustacheBase(blog),
          settings_html: `<h1>Settings</h1>
<form class="settings">
    <fieldset>
        <h2>Colors</h2>
        <div>
            <label for="theme-select">Color Theme</label>
            <select id="theme-select" onchange="updateTheme()">
                <option class="system" value="">Use System Theme</option>
                <option class="dark-mode" value="dark">Dark Theme</option>
                <option class="light-mode" value="light">Light Theme</option>
            </select>
        </div>
    </fieldset>
    <fieldset>
        <h2>Fonts</h2>
        <div>
            <label for="font-family-select">Text Font</label>
            <select id="font-family-select" onchange="updateFont()">
                <option class="default-text-font" value="">Default</option>
                <option class="open-dyslexic-text-font" value="open-dyslexia">Open Dyslexia</option>
                <option class="arial-text-font" value="arial">Arial</option>
                <option class="comic-sans-text-font" value="comic-sans">Comic Sans</option>
            </select>
        </div>
        <div>
            <label for="mono-font-family-select">Code Font</label>
            <select id="mono-font-family-select" onchange="updateCodeFont()">
                <option class="default-code-font" value="">Consolas</option>
                <option class="open-dyslexic-code-font" value="open-dyslexia-mono">Open Dyslexia Mono</option>
                <option class="courier-new-code-font" value="courier-new">Courier New</option>
            </select>
        </div>
    </fieldset>
    <fieldset>
        <h2>Text Rendering</h2>
        <div>
            <input onchange="updateLetterSpacing()" type="checkbox" id="increased-letter-space"><label for="increased-letter-space"> Increased Letter Spacing</label>
        </div>
        <div>
            <input onchange="updateWordSpacing()" type="checkbox" id="increased-word-space"><label for="increased-word-space"> Increased Word Spacing</label>
        </div>
        <div>
            <input onchange="updateLineSpacing()" type="checkbox" id="increased-line-height"><label for="increased-line-height"> Increased Line Spacing</label>
        </div>
        <div>
            <input onchange="updateParagraphSpacing()" type="checkbox" id="increased-paragraph-spacing"><label for="increased-paragraph-spacing"> Increased Paragraph Spacing</label>
        </div>
        <div data-need-vars="true" class="range-div desktop-only">
            <input onchange="updateFontSize()" type="range" id="render-font-size" value="100" min="100" max="200" step="10"><label for="render-font-size"> Text Size</label>
        </div>
    </fieldset>
</form>
<script>
function updateTheme() {
    localStorage.setItem('theme', document.getElementById('theme-select').value)
    loadSettings()
}

function updateFont() {
    localStorage.setItem('font', document.getElementById('font-family-select').value)
    loadSettings()
}

function updateCodeFont() {
    localStorage.setItem('codeFont', document.getElementById('mono-font-family-select').value)
    loadSettings()
}

function updateLetterSpacing() {
    localStorage.setItem('increaseLetterSpace', document.getElementById('increased-letter-space').checked)
    loadSettings()
}

function updateWordSpacing() {
    localStorage.setItem('increaseWordSpace', document.getElementById('increased-word-space').checked)
    loadSettings()
}

function updateLineSpacing() {
    localStorage.setItem('increaseLineHeight', document.getElementById('increased-line-height').checked)
    loadSettings()
}

function updateParagraphSpacing() {
    localStorage.setItem('increaseParagraphSpace', document.getElementById('increased-paragraph-spacing').checked)
    loadSettings()
}

function updateFontSize() {
    localStorage.setItem('fontSize', document.getElementById('render-font-size').value)
    loadSettings()
}

window.addEventListener('load', function () {
    if (!window.CSS || !window.CSS.supports('color', 'var(--fake-var)')) {
        var toHide = document.querySelectorAll('[data-need-vars]')
        for (var i = 0; i < toHide.length; ++i) {
            toHide[i].style.setProperty('display', 'none')
        }
    }
    
    document.getElementById('font-family-select').value = localStorage.getItem('font') || ''
    document.getElementById('mono-font-family-select').value = localStorage.getItem('codeFont') || ''
    document.getElementById('increased-letter-space').checked = localStorage.getItem('increaseLetterSpace') === 'true'
    document.getElementById('increased-word-space').checked = localStorage.getItem('increaseWordSpace') === 'true'
    document.getElementById('increased-line-height').checked = localStorage.getItem('increaseLineHeight') === 'true'
    document.getElementById('increased-paragraph-spacing').checked = localStorage.getItem('increaseParagraphSpace') === 'true'
    document.getElementById('render-font-size').value = localStorage.getItem('fontSize') || 100
    document.getElementById('theme-select').value = localStorage.getItem('theme') || ''
})
</script>`,
        }),
      },
    ]),
  ];
}

function writeFiles([blog, files]) {
  console.log(
    `\n${blog.blogTitle}: Writing ${files.length + blog.assets.stylesheets.length} Files\n`
  );
  const baseDir = blog.outDir.split('/');
  const save = (file) => {
    console.log(`${blog.blogTitle}: Writing file: ${file.fileName}`);
    const fileArr = file.fileName.split('/');
    const dir = [...baseDir, ...fileArr.slice(0, -1)];
    return mkdirp(path.join(...dir)).then(() =>
      writeFile(path.join(...dir, fileArr.slice(-1)[0]), file.content)
    );
  };
  return mkdirp(path.join(...baseDir)).then((_) =>
    Promise.all([
      ...files.map(save),
      ...blog.assets.stylesheets.map(save),
      blog.fontsFolder
        ? mkdirp(path.join(...baseDir, ...blog.fontsFolder.split('/'))).then(() =>
            copy(
              path.join('data', ...blog.fontsFolder.split('/')),
              path.join(...baseDir, ...blog.fontsFolder.split('/').slice(1))
            )
          )
        : Promise.resolve(),
      blog.imgFolder
        ? mkdirp(path.join(...baseDir, ...blog.imgFolder.split('/'))).then(() =>
            copy(
              path.join('data', ...blog.imgFolder.split('/')),
              path.join(...baseDir, ...blog.imgFolder.split('/').slice(1))
            )
          )
        : Promise.resolve(),
    ])
  );
}

function createBibliography(references) {
  const refs = Object.entries(references)
    .sort(([_1, left], [_2, right]) => left.id - right.id)
    .map(([refId, reference]) => {
      switch (reference.type) {
        case 'web':
          return webReference(refId, reference);
        case 'gov-pub':
          return govPublication(refId, reference);
        default:
          throw `Invalid reference type ${reference.type} for reference ${refId}`;
      }
    });

  if (refs.length === 0) {
    return '';
  }

  return `<a class="anchor"></a><section class="article-bibliography"><h2 tabindex="-1" id="__bibliography">Bibliography</h2><ul>${refs.join(
    ''
  )}</ul></section>`;
}

function webReference(refId, reference) {
  const primaryLink = reference.link || reference.webarchive;
  let webArchiveLink = reference.webarchive || '';
  if (webArchiveLink === primaryLink) {
    webArchiveLink = '';
  }
  const pageTitle = '' + (reference['page-title'] || '');
  const websiteTitle = '' + (reference['website-title'] || '');
  const accessDate = reference['access-date'] ? reference['access-date'] : '';
  const names = '' + (reference['names'] || '');

  if (!primaryLink) {
    throw `Missing primary link for reference ${reference.id}`;
  }

  const spans = [];
  let titleId = '';
  let title = '';

  spans.push(`<span class="ref-id">[${reference.id}] </span>`);
  if (names) {
    spans.push(`<span id="${reference.id}--names" class="ref-name">${names}. </span>`);
    titleId = `${reference.id}--names`;
    title = names;
  }
  if (pageTitle) {
    spans.push(`<span id="${reference.id}--title" class="ref-page-title">${pageTitle}. </span>`);
    titleId = `${reference.id}--title`;
    title = pageTitle;
  }
  if (websiteTitle) {
    spans.push(
      `<span id="${reference.id}--website" class="ref-website-title">${websiteTitle} </span>`
    );
    if (!pageTitle) {
      titleId = `${reference.id}--website`;
      title = websiteTitle;
    }
  }
  spans.push(' [Online].');
  if (!titleId) {
    console.error(
      `Invalid bibliographic reference ${reference.id}! Must have a list of names, a title, or a website title!`
    );
    throw 'Invalid reference';
  }
  if (primaryLink) {
    spans.push(`
<span class="ref-page-link">Available: ${primaryLink}. </span>`);
  } else if (webArchiveLink) {
    spans.push(`<span class="webarchive">Available: ${webArchiveLink}. </span>`);
  }
  if (accessDate) {
    spans.push(`<span class="ref-access-date">Accessed ${accessDate}. </span>`);
  }

  return `<li><a id="__bilbiography__ref--${refId}" href="${
    primaryLink || webArchiveLink
  }" target="_blank" aria-labelledby="${titleId}">${spans.join('')}</a></li>`;
}

function govPublication(refId, reference) {
  const primaryLink = reference.link || reference.webarchive;
  let webArchiveLink = reference.webarchive || '';
  if (webArchiveLink === primaryLink) {
    webArchiveLink = '';
  }

  const title = '' + (reference['title'] || '');
  const author = '' + (reference['author'] || '');
  const docId = '' + (reference['document-id'] || '');
  const agency = '' + (reference['agency'] || '');
  const place = '' + (reference['place'] || '');
  const year = '' + (reference['year'] || '');
  const publishLoc = '' + (reference['pub-loc'] || '');
  const pages = '' + (reference['pages'] || '');

  if (!primaryLink) {
    throw `Missing link for reference ${reference.id}`;
  }

  const spans = [];
  let titleId = '';

  spans.push(`<span class="ref-id">[${reference.id}] </span>`);
  if (author) {
    spans.push(`<span id="${reference.id}--author" class="ref-name">${author}. </span>`);
    titleId = `${reference.id}--author`;
  }
  if (agency) {
    spans.push(`<span id="${reference.id}--agency" class="ref-name">${agency}. </span>`);
    titleId = `${reference.id}--agency`;
  }
  if (title) {
    spans.push(`<span id="${reference.id}--title" class="ref-title">${title}</span>`);
    titleId = `${reference.id}--title`;
  }
  if (!titleId) {
    console.error(
      `Invalid bibliographic reference ${reference.id}! Must have a list of names, a title, or a website title!`
    );
    throw 'Invalid reference';
  }

  if (publishLoc) {
    spans.push(`, ${publishLoc}`);
  }

  if (year) {
    spans.push(`, year ${publishLoc}`);
  }

  spans.push(` [Online].`);

  if (pages) {
    spans.push(` Pages ${pages}.`);
  }

  if (docId || primaryLink) {
    spans.push(' Available:');
  }
  if (docId) {
    spans.push(` <span id="${reference.id}--doi" class="doi">${docId}</span>`);
  }
  if (primaryLink) {
    spans.push(` <span class="ref-page-link">${primaryLink}. </span>`);
  } else if (webArchiveLink) {
    spans.push(` <span class="webarchive">${webArchiveLink}. </span>`);
  }

  return `<li><a id="__bilbiography__ref--${refId}" href="${
    primaryLink || webArchiveLink
  }" target="_blank" aria-labelledby="${titleId}">${spans.join('')}</a></li>`;
}

function generateRssFeed([blog, files]) {
  const rssFile = {
    fileName: 'rss.xml',
    content: `<?xml version="1.0" encoding="UTF-8" ?>
<rss version="2.0">
<channel>
  <title>${blog.blogTitle}</title>
  <link>${blog.baseUrl}</link>
  <description>${blog.description}</description>
  <language>en-us</language> 
  ${blog.categories.map((c) => `<category>${c}</category>`).join('')}
  ${blog.articles.map(articleToRss(blog)).join('')}
</channel>

</rss> `,
  };
  return [blog, files.concat([rssFile])];
}

function articleToRss(blog) {
  return (article) => {
    return `<item>
<pubDate>${format(article.time, 'E, dd LLL yyyy HH:mm:ss XXXX')}</pubDate>
<title>${article.title}</title>
<link>${blog.baseUrl}${articleUri(article)}</link>
<description>${article.description}</description>
</item>`;
  };
}

module.exports = {
  generateBlogHtml,
  setMathJaxSvgContext
};
