MathJax.typesetPromise().then(function () {
    for(const elem of document.querySelectorAll('.math-fallback')) {
        elem.style['display'] = 'none'
    }
    for(const elem of document.querySelectorAll('.math-initial')) {
        elem.classList.remove('math-initial')
    }
}).catch(function (err) {
    console.error(err.message)
});