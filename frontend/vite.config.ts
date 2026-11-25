import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// To point at another backend edit BACKEND below.
// The C++ backend (Server(8080)) serves REST under /api and upgrades WebSocket on any path.
// The proxy keeps everything same-origin in dev (no CORS), mirroring infra/nginx.conf in prod.
const BACKEND = 'http://127.0.0.1:8080';

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    host: true,   // listen on the LAN too, so other laptops can open http://<this laptop's IP>:5173
    proxy: {
      '/api': { target: BACKEND, changeOrigin: true },
      '/health': { target: BACKEND, changeOrigin: true },
      '/ws': { target: BACKEND, ws: true, changeOrigin: true },
    },
  },
});
