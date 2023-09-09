const Generator = require('./src/generator')
const path = require('path')

const {promises: {readdir}} = require('fs')

const getDirectories = async source =>
    (await readdir(source, {withFileTypes: true}))
        .filter(dirent => dirent.isDirectory())
        .map(dirent => dirent.name)

async function main() {
    const MathJax = await require('mathjax').init({
        options: {
            enableAssistiveMml: true,
            enableEnrichment: true,   // false to disable enrichment
            sre: {
                speech: 'shallow',         // or 'shallow', or 'deep'
                domain: 'mathspeak',    // speech rules domain
                style: 'default',       // speech rules style
                locale: 'en'            // the language to use (en, fr, es, de, it)
            }
        },
        loader: {
            // paths: {mathjax: 'mathjax-full/es5'},
            // source: require('mathjax-full/components/src/source').source,
            require: require,
            load: ['input/tex', 'output/svg']
        },
        tex: {
            packages: ['base', 'autoload', 'require', 'ams', 'newcommand']
        },
        chtml: {
            // fontURL: require('mathjax-full/es5/output/chtml/fonts/woff-v2')
        }
    });

    Generator.setMathJaxSvgContext(MathJax)

    getDirectories(path.join(__dirname, 'data'))
        .then(
            directories => directories.map(
                directory => Generator.generateBlogHtml(
                    path.join(__dirname, 'data', directory, 'config.xml'),
                    directory
                )
            )
        )
}

main()