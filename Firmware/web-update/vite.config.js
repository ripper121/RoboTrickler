import { defineConfig } from "vite";

export default defineConfig({
  // Relative asset paths work on a GitHub project page and on localhost.
  base: "./",
  build: {
    // Keep esptool's dynamically loaded chip definitions in the main bundle.
    // Otherwise a cached bundle can request a hashed chunk removed by a newer
    // atomic GitHub Pages deployment.
    rollupOptions: {
      output: {
        inlineDynamicImports: true,
      },
    },
  },
});
