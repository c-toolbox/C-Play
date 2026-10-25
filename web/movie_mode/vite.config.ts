import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

// C-Play serves the build at /movie/ (data/web/movie_mode) and the API at the root.
// In development (pnpm dev) the API calls go to a running C-Play, CPLAY_URL or localhost:7007.
const cplay = process.env.CPLAY_URL ?? 'http://127.0.0.1:7007';
const api = ['/events', '/movie_state', '/movie_cue', '/position', '/duration', '/play', '/pause', '/stop', '/seek', '/movie_preview', '/fade_image_down', '/fade_image_up', '/fade_duration'];

export default defineConfig({
  base: './',
  plugins: [svelte()],
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    target: 'es2022',
  },
  server: {
    host: true,
    proxy: Object.fromEntries(api.map((path) => [path, { target: cplay, changeOrigin: true }])),
  },
});
