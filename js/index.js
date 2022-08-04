'use strict';
import Module from './ffvmaf_wasm_lib.js';

let ffModule;
Module().then(module => {
    ffModule = module;
    console.log('ffModule loaded');
    console.log('Vmaf version is ' + ffModule.getVmafVersion());
}).catch(e => {
    console.log('Module() error: ' + e);
});

let videoUrl;

const input = document.getElementById('input');
const button = document.getElementById('button');
input.onchange = function() {
    const blobUrl = URL.createObjectURL(input.files[0]);
    videoUrl = blobUrl.substring(blobUrl.indexOf('blob:') + 5);
}

button.onclick = function() {
    ffModule.computeVmaf(videoUrl, videoUrl);
}

