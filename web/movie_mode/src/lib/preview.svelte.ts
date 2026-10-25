import { ApiError, api, type PreviewFeed } from './api';

/** False when the browser cannot receive the pictures (they need WebRTC, there is no fallback). */
export const webRtcSupported = typeof RTCPeerConnection !== 'undefined';

const retryMs = 2000;

// The offer is sent once with all candidates (C-Play answers without trickle ICE). On a LAN the
// host candidates are there at once; whatever came in time is enough otherwise.
function gathered(pc: RTCPeerConnection, timeoutMs: number) {
  if (pc.iceGatheringState === 'complete') return Promise.resolve();
  return new Promise<void>((resolve) => {
    const done = () => {
      pc.removeEventListener('icegatheringstatechange', check);
      clearTimeout(timer);
      resolve();
    };
    const check = () => {
      if (pc.iceGatheringState === 'complete') done();
    };
    const timer = setTimeout(done, timeoutMs);
    pc.addEventListener('icegatheringstatechange', check);
  });
}

/**
 * One picture from C-Play: receive-only H.264 video over WebRTC, never audio. Reconnects while
 * started; C-Play only encodes a feed while somebody receives it.
 */
export class PreviewStream {
  stream = $state<MediaStream | null>(null);
  state = $state<'idle' | 'connecting' | 'connected' | 'failed'>('idle');
  error = $state('');

  #pc: RTCPeerConnection | null = null;
  #id = '';
  #wanted = false;
  #retry: ReturnType<typeof setTimeout> | undefined;
  #attempt = 0;

  constructor(readonly feed: PreviewFeed) {}

  start() {
    if (this.#wanted || !webRtcSupported) return;
    this.#wanted = true;
    void this.#connect();
  }

  stop() {
    this.#wanted = false;
    clearTimeout(this.#retry);
    this.#teardown();
    this.state = 'idle';
    this.error = '';
  }

  #teardown() {
    this.#attempt++;
    if (this.#id) api.previewClose(this.#id);
    this.#id = '';
    this.#pc?.close();
    this.#pc = null;
    this.stream = null;
  }

  #fail(message: string) {
    this.#teardown();
    this.state = 'failed';
    this.error = message;
    clearTimeout(this.#retry);
    if (this.#wanted) this.#retry = setTimeout(() => void this.#connect(), retryMs);
  }

  async #connect() {
    if (!this.#wanted) return;
    this.#teardown();
    const attempt = this.#attempt;
    this.state = 'connecting';
    const pc = new RTCPeerConnection({ iceServers: [] });
    this.#pc = pc;
    try {
      const transceiver = pc.addTransceiver('video', { direction: 'recvonly' });
      // C-Play only sends H.264; offering just that keeps the negotiation simple.
      const h264 = RTCRtpReceiver.getCapabilities?.('video')?.codecs.filter((codec) => /h264/i.test(codec.mimeType));
      if (h264?.length && transceiver.setCodecPreferences) transceiver.setCodecPreferences(h264);

      pc.ontrack = (event) => {
        if (attempt === this.#attempt) this.stream = event.streams[0] ?? new MediaStream([event.track]);
      };
      pc.onconnectionstatechange = () => {
        if (attempt !== this.#attempt) return;
        if (pc.connectionState === 'connected') {
          this.state = 'connected';
          this.error = '';
        } else if (pc.connectionState === 'failed' || pc.connectionState === 'closed') {
          this.#fail('The picture connection was lost');
        }
      };

      await pc.setLocalDescription(await pc.createOffer());
      await gathered(pc, 1500);
      if (attempt !== this.#attempt) return;
      const answer = await api.previewOffer(this.feed, pc.localDescription!.sdp);
      if (attempt !== this.#attempt) {
        api.previewClose(answer.id);
        return;
      }
      this.#id = answer.id;
      await pc.setRemoteDescription({ type: 'answer', sdp: answer.sdp });
    } catch (error) {
      if (attempt !== this.#attempt) return;
      this.#fail(error instanceof ApiError || error instanceof Error ? error.message : 'No picture from C-Play');
    }
  }
}
