'use strict';


const worker = new Worker('./worker.js', {type: 'module'});
const input = document.getElementById('input');
const button = document.getElementById('button');
input.onchange = function() {
    console.log("Input changed");
}

button.onclick = function() {
    console.log("Button clicked");
    if (input.files.length > 0) {
        const f = input.files[0];
        worker.postMessage([f]);
    }
}

