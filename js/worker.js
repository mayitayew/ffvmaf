import Module from './ffvmaf_wasm_lib.js';

let ffModule;

const renderVmafScore = () => {
    console.log("Render vmaf score called");
}

const importObject = {
    env: {
        renderVmafScore: renderVmafScore,
    }
};

Module(importObject).then(module => {
    ffModule = module;
    console.log('ffModule loaded');
    console.log('Vmaf version is ' + ffModule.getVmafVersion());
}).catch(e => {
    console.log('Module() error: ' + e);
});


onmessage = function(e) {
    console.log("Received message. Vmaf version is " + ffModule.getVmafVersion());
    const f = e.data[0];

    ffModule.FS.mkdir('/wjzm');
    ffModule.FS.mount(ffModule.WORKERFS, {files: [f]}, '/wjzm');
    if (e.data.length > 0) {
        console.log("File is ", f.name);
        ffModule.readFile('/wjzm/' + f.name);
        ffModule.renderScore();
    }
}

