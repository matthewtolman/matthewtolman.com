module.exports = hljs => {
    return {
        keywords: {
            keyword: 'package import proc enum return',
            literal: 'int nil'
        },
        contains: [
            {
                scope: 'string',
                begin: '"', end: '"'
            },
            hljs.C_LINE_COMMENT_MODE,
            hljs.C_BLOCK_COMMENT_MODE,
            {
                className: 'string',
                variants: [
                    hljs.QUOTE_STRING_MODE,
                ]
            },
            {
                className: 'number',
                variants: [
                    hljs.C_NUMBER_MODE,
                ]
            },
        ]
    }

}