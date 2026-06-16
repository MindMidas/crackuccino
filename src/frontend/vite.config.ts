import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";

const runtime = globalThis as { process?: { env?: Record<string, string | undefined> } };
const base = runtime.process?.env?.APP_BASE_PATH || "/";

export default defineConfig({
  base,
  plugins: [react(), tailwindcss()],
  server: {
    host: "127.0.0.1",
    port: 5173,
    proxy: {
      "/api": "http://127.0.0.1:8000",
    },
  },
});
