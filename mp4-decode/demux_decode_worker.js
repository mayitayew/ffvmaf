importScripts('./mp4box.all.min.js');
importScripts('./mp4_demuxer.js');
// importScripts('./ffvmaf_wasm_lib.js');
// importScripts('./video_helper.js');
//
// var ffVmafModule;


self.addEventListener('message', function(e) {

  let offscreen = e.data.canvas;
  let url = e.data.uri;
  let ctx = offscreen.getContext('2d');
  let startTime = 0;
  let frameCount = 0;

  let demuxer = new MP4Demuxer(url);

  function getFrameStats(vmafScore) {
      let now = performance.now();
      let fps = "";

      if (frameCount++) {
        let elapsed = now - startTime;
        fps = " (" + (1000.0 * frameCount / (elapsed)).toFixed(0) + " fps)"
      } else {
        // This is the first frame.
        startTime = now;
      }

      return "Extracted " + frameCount + " frames" + fps + " and Vmaf score is " + vmafScore;
  }

  let decoder = new VideoDecoder({
    output : frame => {

      console.log("Decoded frame");
     /* const buffer_length = frame.allocationSize();
      console.log("Duration is ", frame.duration);
      console.log("Timestamp is ", frame.timestamp);
      let buffer = new Uint8Array(buffer_length);
      let layout = frame.copyTo(buffer);
      var videoFrameBuffer = new HeapVideoBuffer(Module, buffer_length);
      videoFrameBuffer.getVideoData().set(buffer);

      Module.readForVmaf(videoFrameBuffer.getHeapAddress(), videoFrameBuffer.getHeapAddress());*/

      ctx.drawImage(frame, 0, 0, offscreen.width, offscreen.height);

      // Close ASAP.
      frame.close();

      // Draw some optional stats.
      ctx.font = '20px sans-serif';
      ctx.fillStyle = "#ffffff";
      ctx.fillText(getFrameStats("unknown"), 20, 20, offscreen.width);
    },
    error : e => console.error(e),
  });

  demuxer.getConfig().then((config) => {
    offscreen.height = config.codedHeight;
    offscreen.width = config.codedWidth;

    decoder.configure(config);
    demuxer.start((chunk) => { console.log("Demuxed chunk"); })
  });
})
