import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  base: process.env.GITHUB_PAGES ? '/line-follower/' : '/',
  plugins: [react()],
  server: {
    host: true,
  },
});
