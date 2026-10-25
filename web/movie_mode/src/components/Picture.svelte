<script lang="ts">
  import { webRtcSupported, type PreviewStream } from '../lib/preview.svelte';

  interface Props {
    preview: PreviewStream;
    /** Shown over the picture, e.g. where the cue starts. */
    caption?: string;
    /** What C-Play reports about this picture, e.g. no picture at the position. */
    problem?: string;
  }

  let { preview, caption = '', problem = '' }: Props = $props();

  let video: HTMLVideoElement | undefined = $state();
  let hasFrame = $state(false);

  $effect(() => {
    if (!video) return;
    const stream = preview.stream;
    video.srcObject = stream;
    hasFrame = false;
    // Muted as well: the stream has no audio, and muted video may autoplay everywhere.
    if (stream) void video.play().catch(() => {});
  });

  const status = $derived(
    !webRtcSupported
      ? 'This browser has no WebRTC, so it cannot show pictures'
      : problem
        ? problem
        : preview.state === 'failed'
          ? preview.error || 'No picture from C-Play'
          : !hasFrame
            ? 'Waiting for the picture…'
            : '',
  );
</script>

<div class="picture" class:empty={!hasFrame}>
  <video
    bind:this={video}
    muted
    autoplay
    playsinline
    disablepictureinpicture
    onloadeddata={() => (hasFrame = true)}
    onresize={() => (hasFrame = true)}
  ></video>
  {#if status}
    <div class="status" class:problem={!!problem || preview.state === 'failed' || !webRtcSupported}>{status}</div>
  {/if}
  {#if caption && hasFrame}
    <div class="caption">{caption}</div>
  {/if}
</div>

<style>
  .picture {
    position: relative;
    display: grid;
    place-items: center;
    border-radius: 8px;
    overflow: hidden;
    background: var(--frame);
    height: var(--picture-height, 100%);
    min-height: 6rem;
  }
  /* Out of the flow, so the size of the video never changes the layout. */
  video {
    position: absolute;
    inset: 0;
    width: 100%;
    height: 100%;
    object-fit: contain;
    display: block;
  }
  .empty video {
    visibility: hidden;
  }
  .status {
    position: absolute;
    inset: auto 0.6rem;
    text-align: center;
    color: #c9ced8;
    font-size: 0.9rem;
  }
  .status.problem {
    color: #f2b84b;
  }
  .caption {
    position: absolute;
    left: 0.5rem;
    bottom: 0.5rem;
    padding: 0.15rem 0.5rem;
    border-radius: 6px;
    background: rgb(0 0 0 / 0.6);
    color: #fff;
    font-size: 0.8rem;
    font-variant-numeric: tabular-nums;
  }
</style>
