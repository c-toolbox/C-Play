import React from 'react';
import useBaseUrl from '@docusaurus/useBaseUrl';

/**
 * C-Play 2.4 demo video, shown on the home page in place of a static render image.
 * Uses useBaseUrl() so the asset paths work with any configured baseUrl
 * (e.g. /C-Play/ for GitHub Pages) and with DOCS_BASE_URL overrides.
 * The AV1 WebM is skipped by browsers that cannot decode AV1 (e.g. Safari on
 * most iOS devices), which fall back to the H.264 MP4.
 */
function DemoVideo(): React.JSX.Element {
  return (
    <video
      controls
      playsInline
      preload="metadata"
      poster={useBaseUrl('assets/Cplay-v2-4-demo-poster.jpg')}
    >
      <source
        src={useBaseUrl('assets/Cplay-v2-4-demo.webm')}
        type='video/webm; codecs="av01.0.08M.08"'
      />
      <source
        src={useBaseUrl('assets/Cplay-v2-4-demo.mp4')}
        type='video/mp4; codecs="avc1.640029"'
      />
      Your browser does not support the video tag.
    </video>
  );
}

export default DemoVideo;
