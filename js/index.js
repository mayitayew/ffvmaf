'use strict';
// import Module from './ffvmaf_wasm_lib.js';
//
// let ffModule;
// Module().then(module => {
//     ffModule = module;
//     console.log('ffModule loaded');
//     console.log('Vmaf version is ' + ffModule.getVmafVersion());
// }).catch(e => {
//     console.log('Module() error: ' + e);
// });

let videoUrl;

const worker = new Worker('./worker.js', {type: 'module'});
const input = document.getElementById('input');
const button = document.getElementById('button');
input.onchange = function() {
    console.log("Input changed");
}

button.onclick = function() {
    if (input.files.length > 0) {
        const f = input.files[0];
        worker.postMessage([f]);
    }
}

