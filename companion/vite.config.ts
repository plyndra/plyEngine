import { defineConfig } from "vite";

// The build must be plain static files: the plyEngine host serves them
// directly from companion/dist over its own embedded HTTP server.
export default defineConfig({
  base: "./",
  build: {
    outDir: "dist",
    emptyOutDir: true,
    target: "es2020",
    assetsInlineLimit: 0,
    rollupOptions: {
      output: {
        // Predictable file names so the host can serve them without a manifest.
        entryFileNames: "app.js",
        chunkFileNames: "app-[name].js",
        assetFileNames: "[name][extname]",
      },
    },
  },
  server: {
    host: true,
  },
});
