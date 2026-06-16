export const GITHUB_URL = "https://github.com/MindMidas/crackuccino";
const APP_BASE = import.meta.env.BASE_URL.replace(/\/$/, "");
export const assetUrl = (path: string) => `${APP_BASE}${path.startsWith("/") ? path : `/${path}`}`;
export const REPORT_URL = `${assetUrl("/assets/report.pdf")}#navpanes=0`;
