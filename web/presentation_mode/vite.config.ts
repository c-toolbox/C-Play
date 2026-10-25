import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

// C-Play serves the build at /presenter/ (data/web/presentation_mode) and the API at the root.
// In development (pnpm dev) the API calls go to a running C-Play, CPLAY_URL or localhost:7007.
const cplay = process.env.CPLAY_URL ?? 'http://127.0.0.1:7007';
const api = ['/events', '/presentation_state', '/presentation_save', '/slide_thumbnail', '/slide_notes',
  '/slide_next', '/slide_previous', '/load_from_slides'];

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
