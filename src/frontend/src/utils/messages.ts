export interface SectionMessage {
  type: "error" | "success";
  title: string;
  message: string;
}

export type SectionKey = "encrypt" | "decrypt" | "run" | "dictionary" | "upload" | "create";

function isRateLimitError(reason: unknown): boolean {
  return reason instanceof Error && "status" in reason && reason.status === 429;
}

export function errorMessage(reason: unknown, fallback: string): string {
  if (isRateLimitError(reason)) {
    return "Limited compute resources are cooling down. Please wait a moment before starting another request.";
  }
  return reason instanceof Error ? reason.message : fallback;
}

export function sectionError(title: string, message: string): SectionMessage {
  return { type: "error", title, message };
}

export function sectionSuccess(title: string, message: string): SectionMessage {
  return { type: "success", title, message };
}
