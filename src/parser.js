const { readFile } = require('fs-extra');
const xmlParse = require('fast-xml-parser').parse;
const path = require('path');
const sassRender = require('sass').render;
const { ArticleTokenType } = require('./types');
const uglifyJs = require('uglify-js')
const cleanCss = require('clean-css')
const fs = require("fs");

function parseBlog(fileName, blogId) {
  return readFile(fileName, 'UTF-8')
    .then((s) =>
      xmlParse(s, { attrNodeName: '_attr', attributeNamePrefix: '', ignoreAttributes: false })
    )
    .then((x) => [
      x,
      {
        id: blogId,
        description: x.blog._attr.description,
        copyrightHolder: x.blog._attr['copyright-holder'],
        icon: fs.readFileSync(path.join(path.dirname(fileName), x.blog._attr.icon)),
        settingsTemplate: x.blog._attr['settings-template'] || 'settings-templ.mustache',
        fontsFolder: path.join(blogId, 'fonts'),
        baseUrl: x.blog._attr['base-url'],
        assets: { stylesheets: [], scripts: [], copy: [] },
        blogTitle: x.blog._attr.title,
        outDir: path.join('out', blogId),
        articles: [],
        imgFolder: x.blog._attr['img-folder'] || '',
        authors: {},
        articleTemplate: '',
        listTemplate: '',
        categories: [],
      },
    ])
    .then(addTemplates)
    .then(addAuthors)
    .then(addCategories)
    .then(addArticles)
    .then(addAssets)
    .then((x) => x[1]);
}

async function addAssets([xml, blog]) {
  const cssMinifier = new cleanCss({
    level: 2,
    returnPromise: false
  })
  const assetsArray = await Promise.all(
      Object.entries(xml.blog.assets)
          .map(([type, assets]) => [type, Array.isArray(assets) ? assets : [assets]])
          .flatMap(([type, assets]) => {
            switch (type) {
              case 'stylesheet':
                return Promise.all(
                    assets.map((a) => {
                      const toFile = (content) => {
                        return ({
                          fileName: type + '/' + path.parse(a._attr.src).name + '.css',
                          content: cssMinifier.minify(content).styles,
                        })
                      };
                      const fileName = path.join('data', blog.id, ...a._attr.src.split('/'));
                      switch (a._attr.type) {
                        case 'sass':
                        case 'scss':
                          return new Promise((resolve, reject) =>
                              sassRender(
                                  {
                                    file: fileName,
                                  },
                                  (err, res) => {
                                    if (err) {
                                      return reject(err);
                                    }
                                    resolve(res.css.toString('utf-8'));
                                  }
                              )
                          ).then(toFile);
                        default:
                          return readFile(fileName, {encoding: 'utf-8', flag: 'r'}).then(toFile);
                      }
                    })
                ).then((stylesheets) => ['stylesheets', stylesheets]);
              case 'script':
                return Promise.all(
                    assets.map((a) => {
                      const toFile = (content) => ({
                        fileName: type + '/' + path.parse(a._attr.src).name + '.js',
                        content: uglifyJs.minify(content).code,
                      });
                      const fileName = path.join('data', blog.id, ...a._attr.src.split('/'));

                      switch (a._attr.type) {
                        case 'js':
                        default:
                          return readFile(fileName, {encoding: 'utf-8', flag: 'r'}).then(toFile)

                      }
                    })
                ).then((scripts) => ['scripts', scripts])
              case 'copy':
                return Promise.all(
                    assets.map((a) => {
                      const fileName = path.join('data', blog.id, ...a._attr.src.split('/'));
                      const toFile = (content) => ({
                        fileName: path.join(...a._attr.src.split('/')),
                        path: fileName,
                      });

                      return toFile(a)
                    })
                ).then((files) => ['to_copy', files])
              default:
                console.warn('Unsupported asset type ' + type);
                return null;
            }
          })
          .filter((t) => t)
  );
  blog.assets = assetsArray.reduce((a, c) => ({...a, [c[0]]: c[1]}), {});
  blog.assets.stylesheets = blog.assets.stylesheets || []
  blog.assets.scripts = blog.assets.scripts || []
  blog.assets.to_copy = blog.assets.to_copy || []
  return [xml, blog];
}

function addTemplates([xml, blog]) {
  let articles = xml.blog.articles._attr;
  return Promise.all([
    readFile(path.join('data', blog.id, ...articles.articleTemplate.split('/')), 'utf-8').then(
      (f) => ['articleTemplate', f]
    ),
    readFile(path.join('data', blog.id, ...articles.listTemplate.split('/')), 'utf-8').then((f) => [
      'listTemplate',
      f,
    ]),
    articles.redirectTemplate
      ? readFile(path.join('data', blog.id, ...articles.redirectTemplate.split('/')), 'utf-8').then(
          (f) => ['redirectTemplate', f]
        )
      : null,
    articles.subscribeTemplate
      ? readFile(
          path.join('data', blog.id, ...articles.subscribeTemplate.split('/')),
          'utf-8'
        ).then((f) => ['subscribeTemplate', f])
      : null,
    readFile(path.join('data', blog.id, ...blog.settingsTemplate.split('/')), 'utf-8').then((f) => [
      'settingsTemplate',
      f,
    ]),
  ]).then((files) => {
    files
      .filter((f) => f)
      .forEach(([key, file]) => {
        blog[key] = file;
      });
    return [xml, blog];
  });
}

function addAuthors([xml, blog]) {
  let authors = xml.blog.authors.author;
  if (!Array.isArray(authors)) {
    authors = [authors];
  }
  return [
    xml,
    {
      ...blog,
      authors: authors
        .map((author) => {
          return {
            id: author._attr.id,
            name: author._attr.name,
            profileImg: author._attr['profile-img'],
            link: author._attr.link
          };
        })
        .reduce((acc, cur) => ({ ...acc, [cur.id]: cur }), {}),
    },
  ];
}

function addCategories([xml, blog]) {
  let categories = xml.blog.categories.category;
  if (!Array.isArray(categories)) {
    categories = [categories];
  }
  return [
    xml,
    {
      ...blog,
      categories: categories.map((category) => {
        return `${category}`;
      }),
    },
  ];
}

function addSeries([xml, blog]) {
  return new Promise((resolve, reject) => {
    const blogArticles = blog.articles
      .map((a) => [a.id, a])
      .reduce((a, c) => ({ ...a, [c[0]]: c[1] }), {});

    let seriesXml = Array.isArray(xml.blog.series) ? xml.blog.series : [xml.blog.series];
    if (seriesXml.length) {
      for (const sXml of seriesXml) {
        let articles = sXml.articleRef;
        if (!articles || !Array.isArray(articles)) {
          continue;
        }
        articles = articles.filter((a) => a._attr.ref && blogArticles[a._attr.ref].published);
        if (articles.length <= 1) {
          continue;
        }

        for (let i = 0; i < articles.length; ++i) {
          const article = blogArticles[articles[i]._attr.ref];
          article.series = { seriesTitle: sXml._attr.title };
          if (i > 0) {
            const prevArticle = blogArticles[articles[i - 1]._attr.ref];
            article.series.previousArticle = {
              id: prevArticle.id,
              title: prevArticle.title,
            };
          }
          if (i + 1 < articles.length) {
            const nextArticle = blogArticles[articles[i + 1]._attr.ref];
            article.series.previousArticle = {
              id: nextArticle.id,
              title: nextArticle.title,
            };
          }
        }
      }
    }
    resolve([xml, blog]);
  });
}

function addArticles([xml, blog]) {
  let articles = xml.blog.articles.article;
  if (!Array.isArray(articles)) {
    articles = [articles];
  }

  return Promise.all(
    articles.map((article) =>
      parseArticle(path.join('data', blog.id, 'articles', ...article._attr.file.split('/'))).then(
        (file) => [article, file]
      )
    )
  )
    .then((articles) =>
      articles.map(([xmlArticle, content]) => {
        return {
          id: xmlArticle._attr.id,
          title: xmlArticle._attr.title,
          author: blog.authors[xmlArticle._attr.author],
          time: new Date(xmlArticle._attr.time),
          content,
          description: xmlArticle.description,
          published: xmlArticle._attr.published != null && (typeof xmlArticle._attr.published) !== 'undefined' ? xmlArticle._attr.published : true,
          tags: [], // TODO: add tags
          assets: {}, // TODO: add article specific assets
        };
      })
    )
    .then((articles) => [xml, { ...blog, articles: articles }]);
}

function parseArticle(fileName) {
  return readFile(fileName, 'UTF-8')
    .then(parse)
    .then((ast) => ({ ...ast, tokens: inflateTree(ast.tokens) }));
}

function parse(str) {
  let headerCount = 0
  const result = {
    tokens: [],
    references: {},
    sections: [],
  };

  const tokenRegex =
    /(?<bold_italic>\*\*\*[\w \t\.,\(\)]+\*\*\*)|(?<bold>\*\*[\w \t\.,\(\)]+\*\*)|(?<italic>\*[\w \t\.,\(\)]+\*)|(?<list>^[^\S\r\n]*(?<list_char>[\*\-$])\s(?<list_text>.*))|((?<header_level>#{1,6})(?<header>.*))|(?<obj_link>\[\[[\w:\-]+]])|(?<link>\[(?<link_text>[^\]]+)\]\((?<link_ref>[^\)]+)\))|(?<ref>\^\[[\w-]+\])|(?<references>@References\n(\s*(?:\*\s*(?:[\w\-]+)(\s*\|\s*(?:[\w\-]+):\s*(?:[^\n]+))*))*)|~(?<elem>(?<elem_tag>\w+)(?<attrs>(::[\w-]+=(:?[^:])*?)+?)?(::(?<content>.+?::~\/\w+))?:;)|(?<code_block>```(?:\w+)\n(?:(.|\n)*?)```)|(?<inline_code>`(?<inline_lang>\<\w+\>)?.*?`)|(?<toc>\[toc\])|(?<p_break>\r?\n\r?\n+)|(?<blockquote>(^>+.*\n)+)|(?<math>\$\$\n(?<math_equat>(\n|.)*?)\n\$\$)|(?<math_inline>\\\((?<math_equat_inline>.*?)\\\))|(?<table>(?<thead>(?:\|(?:[^|\\\n]|\\\||\\\\|\\n)*)+\|)\n(?<tdiv>(?:\|\-\-\-+)+\|)\n(?<trows>(?:(?:\|(?:[^|\\\n]|\\\||\\\\|\\n)*)+\|\n)+))|(?<escaped>\\.)|[\w\s\."',]+?|./gm;

  let match;
  while ((match = tokenRegex.exec(str)) !== null) {
    // This is necessary to avoid infinite loops with zero-width matches
    if (match.index === tokenRegex.lastIndex) {
      tokenRegex.lastIndex++;
    } else if (match.groups.references) {
      result.references = { ...result.references, ...parseReferences(match[0]) };
    } else if (match.groups.blockquote) {
      const subquote = match[0].replace(/^>(?: *)/gm, '');
      const data = inflateTree(parse(subquote).tokens)
      result.tokens.push({
        type: ArticleTokenType.BLOCK_QUOTE,
        data,
      });
    } else if (match.groups.bold_italic) {
      result.tokens.push({
        type: ArticleTokenType.BOLD_ITALIC,
        data: parse(match[0].slice(3, -3)).tokens,
      });
    } else if (match.groups.bold) {
      result.tokens.push({
        type: ArticleTokenType.BOLD,
        data: parse(match[0].slice(2, -2)).tokens,
      });
    } else if (match.groups.italic) {
      result.tokens.push({
        type: ArticleTokenType.ITALIC,
        data: parse(match[0].slice(1, -1)).tokens,
      });
    } else if (match.groups.obj_link) {
      result.tokens.push({
        type: ArticleTokenType.OBJ_LINK,
        data: [],
        attrs: { ref: (match.groups.obj_link || '').slice(2, -2) },
      });
    } else if (match.groups.header_level && match.groups.header) {
      const count = ++headerCount
      result.sections.push({
        title: (match.groups.header || '').trim(),
        level: match.groups.header_level.length || 1,
        count
      });
      result.tokens.push({
        type: ArticleTokenType.HEADER,
        data: [(match.groups.header || '').trim()],
        attrs: { level: match.groups.header_level.length || 1, count},
      });
    } else if (match.groups.ref) {
      result.tokens.push({
        type: ArticleTokenType.REF,
        data: [],
        attrs: { ref: match.groups.ref.slice(2, -1) },
      });
    } else if (match.groups.link && match.groups.link_ref && match.groups.link_text) {
      result.tokens.push({
        type: ArticleTokenType.LINK,
        data: [match.groups.link_text || ''],
        attrs: { ref: match.groups.link_ref || '' },
      });
    } else if (match.groups.list) {
      result.tokens.push({
        type: ArticleTokenType.LIST,
        data: parse(match.groups.list_text).tokens,
        attrs: {
          ordered: match.groups.list_char === '$',
          leadingWhitespace: match[0].slice(0, match[0].indexOf(match.groups.list_char || '')),
        },
      });
    } else if (match.groups.toc) {
      result.tokens.push({
        type: ArticleTokenType.TABLE_OF_CONTENTS,
        data: [],
      });
    } else if (match.groups.p_break) {
      result.tokens.push({
        type: ArticleTokenType.PARAGRAPH_BREAK,
        data: [],
      });
    } else if (match.groups.escaped) {
      result.tokens.push(match[0].slice(1));
    } else if (match.groups.inline_code) {
      result.tokens.push({
        type: ArticleTokenType.INLINE_CODE,
        data: [match[0].slice(1, -1)],
        attrs: {
          lang: match.groups.inline_lang || 'text'
        }
      });
    } else if (match.groups.elem) {
      const pieces = (match.groups.attrs || '').split('::');
      result.tokens.push({
        type: ArticleTokenType.ELEM,
        data: (match.groups.content) ? [match.groups.content.split(':')[0]] : [],
        attrs: pieces
          .slice(1)
          .map((s) => [s.slice(0, s.indexOf('=')), s.slice(s.indexOf('=') + 1)])
          .reduce((a, c) => ({ ...a, [c[0]]: c[1] }), { tag: match.groups.elem_tag }),
      });
    }
    else if (match.groups.code_block) {
      result.tokens.push(parseCodeBlock(match[0]));
    }
    else if (match.groups.math_inline) {
      result.tokens.push({
        type: ArticleTokenType.MATH,
        data: [],
        attrs: {equation: match.groups.math_equat_inline, inline: true}
      });
    }
    else if (match.groups.math) {
      result.tokens.push({
        type: ArticleTokenType.MATH,
        data: [],
        attrs: {equation: match.groups.math_equat, inline: false}
      });
    }
    else if (match.groups.table) {
      result.tokens.push(parseTable(match.groups.thead, match.groups.trows))
    }
    else {
      const empty = result.tokens.length == 0;
      const last = !empty ? result.tokens[result.tokens.length - 1] : null;
      if (last && typeof last === 'string') {
        result.tokens[result.tokens.length - 1] += match[0];
      } else if (last && match[0] == '\n' && last.type === 'p_break') {
      } else if (last && match[0] == '\n') {
        result.tokens.push({
          type: ArticleTokenType.PARAGRAPH_BREAK,
          data: [],
        });
      } else {
        result.tokens.push(match[0]);
      }
    }
  }
  return result;
}

function parseCodeBlock(str) {
  const regex = /```(?<code_type>\w+)\r?\n(?<code>(.|\n)*)```/gm;
  let m;

  while ((m = regex.exec(str)) !== null) {
    // This is necessary to avoid infinite loops with zero-width matches
    if (m.index === regex.lastIndex) {
      regex.lastIndex++;
    }

    return {
      type: ArticleTokenType.CODE_BLOCK,
      data: [m.groups.code || ''],
      attrs: {
        lang: m.groups.code_type || '',
      },
    };
  }
  return {
    type: ArticleTokenType.CODE_BLOCK,
    data: [],
    attrs: {},
  };
}

function parseReferences(str) {
  const referenceRegex = /\s*\*\s*(?<ref_name>[\w\-]+)(\s*\|\s*([\w\-]+):\s*([^\n\|]+))*/gm;
  const propRegex = /\s*\|\s*(?<prop_name>[\w\-]+):\s*(?<prop_value>[^\n\|]+)/gm;
  let match;
  let res = {};
  let id = 0;
  while ((match = referenceRegex.exec(str)) !== null) {
    // This is necessary to avoid infinite loops with zero-width matches
    if (match.index === referenceRegex.lastIndex) {
      referenceRegex.lastIndex++;
    }

    if (match.groups.ref_name) {
      const refId = match.groups.ref_name || '';
      const reference = {
        id: ++id,
        type: 'other',
      };
      let propMatch;
      while ((propMatch = propRegex.exec(match[0])) !== null) {
        if (propMatch.index === propRegex.lastIndex) {
          propRegex.lastIndex++;
        }

        reference[propMatch.groups.prop_name || ''] = propMatch.groups.prop_value || '';
      }

      res[refId] = reference;
    }
  }

  return res;
}

function parseTableCells(line) {
  const cellRegex = /\|(?<content>(?:[^\\\|]|\\.)+)/gm
  const res = []
  let match
  while ((match = cellRegex.exec(line)) !== null) {
    const inner = match.groups.content
        .replace(/\\\|/g, "|")
        .replace(/\\\\/g, "\\")
        .replace(/\r?\n/g, "~br:;")
        .trim()
    res.push(parse(inner))
  }
  return res
}

function parseTable(tableHead, tableRows) {
  const headCells = parseTableCells(tableHead)
  const rowCells = tableRows.split('\n')
      .filter(r => r)
      .map(parseTableCells)
  return {
    type: ArticleTokenType.TABLE,
    data: [],
    attrs: {head: headCells, body: rowCells}
  }
}

function inflateTree(tokens) {
  const newTokens = [];
  let currentParagraph = {
    type: ArticleTokenType.PARAGRAPH,
    data: [],
  };

  const addTokenAndParagraph = (token) => {
    if (currentParagraph.data.length) {
      newTokens.push(currentParagraph);
      currentParagraph = {
        type: ArticleTokenType.PARAGRAPH,
        data: [],
      };
    }
    if (
      token.type != ArticleTokenType.PARAGRAPH_BREAK &&
      token.type != ArticleTokenType.PARAGRAPH
    ) {
      newTokens.push({
        ...token,
        data: inflateTree(token.data).flatMap((t) =>
          typeof t !== 'string' && t.type === ArticleTokenType.PARAGRAPH ? t.data : [t]
        ),
      });
    }
  };

  const addToken = (token) =>
    currentParagraph.data.push({
      ...token,
      data: inflateTree(token.data).flatMap((t) =>
        typeof t !== 'string' && t.type === ArticleTokenType.PARAGRAPH ? t.data : [t]
      ),
    });

  for (const token of tokens) {
    if (typeof token === 'string') {
      currentParagraph.data.push(token);
    } else {
      switch (token.type) {
        case ArticleTokenType.HEADER:
        case ArticleTokenType.TABLE_OF_CONTENTS:
        case ArticleTokenType.PARAGRAPH_BREAK:
        case ArticleTokenType.CODE_BLOCK:
        case ArticleTokenType.REFERENCE:
        case ArticleTokenType.PARAGRAPH:
        case ArticleTokenType.LIST:
          addTokenAndParagraph(token);
          break;
        case ArticleTokenType.BLOCK_QUOTE:
          currentParagraph.data.push(token)
          break;
        default:
          addToken(token);
      }
    }
  }
  if (currentParagraph.data.length) {
    newTokens.push(currentParagraph);
  }

  return newTokens;
}

module.exports = {
  parseBlog,
};
