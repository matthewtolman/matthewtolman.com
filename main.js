const Generator = require('./src/generator')
const path = require('path')

const {promises: {readdir}} = require('fs')

const getDirectories = async source =>
    (await readdir(source, {withFileTypes: true}))
        .filter(dirent => dirent.isDirectory())
        .map(dirent => dirent.name)

async function main() {
    const MathJax = await require('mathjax').init({
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