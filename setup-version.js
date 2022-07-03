const path = require('path')
const fs = require("fs");
const {exec} = require('child_process');

async function run_command(cmd) {
    return new Promise((resolve, reject) => {
        exec(cmd, function (error, stdout, stderr) {
            if (error) {
                reject(error)
            }
            resolve(stdout)
        });
    })
}

async function setup_version() {
    const fileName = path.join(__dirname, 'generator', 'version.h')

    let version = ''
    const gitCmd = process.argv[2] || 'git'

    let currentFileContents = ''

    try {
        currentFileContents = fs.readFileSync(fileName)
        version = currentFileContents.match(/full\(\) { return "(.*?)"/, fileContents)[1]
    } catch (e) {
    }

    let tag = ''
    try {
        tag = await run_command(`${gitCmd} describe --tags`[gitCmd, 'describe', '--tags'])
    } catch (e) {
    }

    tag = tag || '0.0.0-dev'

    const [major, minor, patch] = tag.split('-')[0].split('.')

    const commit = (await run_command(`${gitCmd} rev-parse HEAD`)).trim()
    const fullVersion = `${major}.${minor}.${patch}/${commit}`

    if (version !== fullVersion) {
        console.log("Updating version.h")
        const headerContents = `// clang-format off
/** WARNING: This file is auto generated! Do not change! **/
#pragma once
namespace calendar {
  struct Version {
    static constexpr const char *full() { return "${fullVersion}"; }
    static constexpr int         major() { return ${major}; }
    static constexpr int         minor() { return ${minor}; }
    static constexpr int         patch() { return ${patch}; }
    static constexpr const char *commit() { return "${commit}"; }
  };
}
// clang-format on`
        fs.writeFileSync(fileName, headerContents)
    }
}

setup_version()