import React from 'react';
import useBaseUrl from '@docusaurus/useBaseUrl';

/**
 * C-Play 2.4 demo video, shown on the home page in place of a static render image.
 * Uses useBaseUrl() so the asset paths work with any configured baseUrl
 * (e.g. /C-Play/ for GitHub Pages) and with DOCS_BASE_URL overrides.
 */
function DemoVideo(): JSX.Element {
  return (
    <video
      controls
      preload="metadata"
      poster={useBaseUrl('assets/Cplay-v2-4-demo-poster.jpg')}
    >
      <source
        src={useBaseUrl('assets/Cplay-v2-4-demo.webm')}
        type="video/webm"
      />
      Your browser does not support the video tag.
    </video>
  );
}

export default DemoVideo;
