const Generator = require('./src/generator')
const path = require('path')
const fs = require('fs')

const {promises: {readdir}} = require('fs')

const getDirectories = async source =>
    (await readdir(source, {withFileTypes: true}))
        .filter(dirent => dirent.isDirectory())
        .map(dirent => dirent.name)

getDirectories(path.join(__dirname, 'data'))
    .then(
        directories => directories.map(
            directory => Generator.generateBlogHtml(
                path.join(__dirname, 'data', directory, 'config.xml'),
                directory
            )
        )
    )