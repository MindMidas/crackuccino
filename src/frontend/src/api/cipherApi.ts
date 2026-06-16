import type {
  BenchmarkStats,
  CustomDictionary,
  DictionaryDetail,
  DictionaryInfo,
  DictionaryListResponse,
  DictionaryUploadRequest,
  DictionaryUploadResponse,
  DecryptRequest,
  DecryptionResult,
  DirectDecryptRequest,
  DirectDecryptResponse,
  EncryptRequest,
  EncryptResponse,
  MpiRankStatus,
  RankState,
  RunHistoryResponse,
  RunSnapshot,
  RunState,
} from "../types/cipher";

const APP_BASE = import.meta.env.BASE_URL.replace(/\/$/, "");
const API_ROOT = `${APP_BASE}/api`;

export class ApiError extends Error {
  readonly status: number;

  constructor(message: string, status: number) {
    super(message);
    this.name = "ApiError";
    this.status = status;
  }
}

export function isApiNotFound(reason: unknown): reason is ApiError {
  return reason instanceof ApiError && reason.status === 404;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function requiredString(record: Record<string, unknown>, key: string): string {
  const value = record[key];
  if (typeof value !== "string") throw new Error(`API response is missing "${key}".`);
  return value;
}

function nullableString(record: Record<string, unknown>, key: string): string | null {
  const value = record[key];
  if (value === null) return null;
  if (typeof value !== "string") throw new Error(`API response has invalid "${key}".`);
  return value;
}

function requiredNumber(record: Record<string, unknown>, key: string): number {
  const value = record[key];
  if (typeof value !== "number" || !Number.isFinite(value)) throw new Error(`API response is missing "${key}".`);
  return value;
}

function nullableNumber(record: Record<string, unknown>, key: string): number | null {
  const value = record[key];
  if (value === null) return null;
  if (typeof value !== "number" || !Number.isFinite(value)) throw new Error(`API response has invalid "${key}".`);
  return value;
}

function parseEncryptResponse(value: unknown): EncryptResponse {
  if (!isRecord(value)) throw new Error("The encryption API returned an invalid response.");
  return {
    plaintext: requiredString(value, "plaintext"),
    inputDictionary: requiredString(value, "inputDictionary"),
    encryptionDictionary: requiredString(value, "encryptionDictionary"),
    ciphertext: requiredString(value, "ciphertext"),
  };
}

function parseDirectDecryptResponse(value: unknown): DirectDecryptResponse {
  if (!isRecord(value)) throw new Error("The direct decrypt API returned an invalid response.");
  return {
    ciphertext: requiredString(value, "ciphertext"),
    inputDictionary: requiredString(value, "inputDictionary"),
    encryptionDictionary: requiredString(value, "encryptionDictionary"),
    plaintext: requiredString(value, "plaintext"),
  };
}

function parseDictionarySource(value: unknown): "builtin" | "custom" {
  if (value === "builtin" || value === "custom") return value;
  throw new Error("The API returned an invalid dictionary source.");
}

function parseDictionaryInfo(value: unknown): DictionaryInfo {
  if (!isRecord(value)) throw new Error("The API returned an invalid dictionary.");
  return {
    id: requiredString(value, "id"),
    label: requiredString(value, "label"),
    source: parseDictionarySource(value.source),
    wordCount: requiredNumber(value, "wordCount"),
  };
}

function parseDictionaryDetail(value: unknown): DictionaryDetail {
  if (!isRecord(value)) throw new Error("The API returned an invalid dictionary detail.");
  const words = Array.isArray(value.words) ? value.words.filter((item): item is string => typeof item === "string") : [];
  return {
    ...parseDictionaryInfo(value),
    words,
    truncated: value.truncated === true,
  };
}

function parseCustomDictionary(value: unknown): CustomDictionary {
  if (!isRecord(value)) throw new Error("The API returned an invalid custom dictionary.");
  const detail = parseDictionaryDetail(value);
  if (detail.source !== "custom") throw new Error("The API returned an invalid custom dictionary source.");
  return {
    ...detail,
    source: "custom",
    content: requiredString(value, "content"),
  };
}

function parseRankState(value: unknown): RankState {
  if (value === "complete" || value === "running" || value === "queued" || value === "canceled") return value;
  throw new Error("The API returned an invalid rank state.");
}

function parseRunState(value: unknown): RunState {
  if (value === "complete" || value === "running" || value === "queued" || value === "failed" || value === "canceled") return value;
  throw new Error("The API returned an invalid run state.");
}

function parseTraceEntry(value: unknown): MpiRankStatus["permutationTrace"][number] | null {
  if (!isRecord(value)) return null;
  return {
    permutation: requiredString(value, "permutation"),
    visited: requiredNumber(value, "visited"),
    prefix: requiredString(value, "prefix"),
  };
}

function parseRankStatus(value: unknown): MpiRankStatus | null {
  if (!isRecord(value)) return null;
  const trace = Array.isArray(value.permutationTrace)
    ? value.permutationTrace.map(parseTraceEntry).filter((item): item is MpiRankStatus["permutationTrace"][number] => item !== null)
    : [];
  return {
    rank: requiredNumber(value, "rank"),
    state: parseRankState(value.state),
    permutations: requiredNumber(value, "permutations"),
    hits: requiredNumber(value, "hits"),
    prefix: requiredString(value, "prefix"),
    samplePermutation: typeof value.samplePermutation === "string" ? value.samplePermutation : "",
    permutationTrace: trace,
    completedTasks: requiredNumber(value, "completedTasks"),
    assignedTasks: requiredNumber(value, "assignedTasks"),
    assignedPermutations: requiredNumber(value, "assignedPermutations"),
  };
}

function parseResult(value: unknown): DecryptionResult | null {
  if (!isRecord(value)) return null;
  return {
    rank: requiredNumber(value, "rank"),
    permutation: requiredString(value, "permutation"),
    plaintext: requiredString(value, "plaintext"),
  };
}

function parseStats(value: unknown): BenchmarkStats {
  if (!isRecord(value)) throw new Error("The API returned invalid benchmark stats.");
  return {
    ranks: requiredNumber(value, "ranks"),
    uniqueLetters: requiredNumber(value, "uniqueLetters"),
    depth: requiredNumber(value, "depth"),
    expectedPermutations: requiredNumber(value, "expectedPermutations"),
    visitedPermutations: requiredNumber(value, "visitedPermutations"),
    validHits: requiredNumber(value, "validHits"),
    runtimeSeconds: requiredNumber(value, "runtimeSeconds"),
    serialEstimateSeconds: requiredNumber(value, "serialEstimateSeconds"),
    speedup: requiredNumber(value, "speedup"),
    efficiencyPercent: requiredNumber(value, "efficiencyPercent"),
    progressPercent: requiredNumber(value, "progressPercent"),
  };
}

function parseRequest(value: unknown): DecryptRequest {
  if (!isRecord(value)) throw new Error("The API returned an invalid run request.");
  return {
    ciphertext: requiredString(value, "ciphertext"),
    plaintext: requiredString(value, "plaintext"),
    dictionary: requiredString(value, "dictionary"),
    ranks: requiredNumber(value, "ranks"),
    depth: nullableNumber(value, "depth"),
  };
}

function parseRunSnapshot(value: unknown): RunSnapshot {
  if (!isRecord(value)) throw new Error("The API returned an invalid run.");
  const results = Array.isArray(value.results) ? value.results.map(parseResult).filter((item): item is DecryptionResult => item !== null) : [];
  const rankStatuses = Array.isArray(value.rankStatuses)
    ? value.rankStatuses.map(parseRankStatus).filter((item): item is MpiRankStatus => item !== null)
    : [];
  return {
    id: requiredString(value, "id"),
    state: parseRunState(value.state),
    createdAt: requiredNumber(value, "createdAt"),
    completedAt: nullableNumber(value, "completedAt"),
    request: parseRequest(value.request),
    results,
    rankStatuses,
    stats: parseStats(value.stats),
    error: nullableString(value, "error"),
  };
}

async function requestJson<T>(path: string, init: RequestInit | undefined, parser: (value: unknown) => T): Promise<T> {
  let response: Response;
  try {
    response = await fetch(`${API_ROOT}${path}`, init);
  } catch {
    throw new Error("Could not reach the Crackuccino API. Start the local server and try again.");
  }
  const body: unknown = await response.json().catch(() => null);
  if (!response.ok) {
    const message = isRecord(body) && typeof body.error === "string" ? body.error : `Request failed with status ${response.status}.`;
    throw new ApiError(message, response.status);
  }
  return parser(body);
}

function postJson<T>(path: string, payload: unknown, parser: (value: unknown) => T): Promise<T> {
  return requestJson(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  }, parser);
}

export function encryptText(request: EncryptRequest): Promise<EncryptResponse> {
  return postJson("/encrypt", request, parseEncryptResponse);
}

export function getDictionaries(): Promise<DictionaryListResponse> {
  return requestJson("/dictionaries", undefined, (value) => {
    if (!isRecord(value) || !Array.isArray(value.dictionaries)) throw new Error("The API returned invalid dictionaries.");
    return { dictionaries: value.dictionaries.map(parseDictionaryInfo) };
  });
}

export function getDictionary(dictionaryId: string): Promise<DictionaryDetail> {
  return requestJson(`/dictionaries/${encodeURIComponent(dictionaryId)}`, undefined, parseDictionaryDetail);
}

export function uploadDictionary(request: DictionaryUploadRequest): Promise<DictionaryUploadResponse> {
  return postJson("/dictionaries", request, (value) => {
    if (!isRecord(value)) throw new Error("The API returned an invalid dictionary upload response.");
    return { dictionary: parseCustomDictionary(value.dictionary) };
  });
}

export function startRun(request: DecryptRequest): Promise<RunSnapshot> {
  return postJson("/runs", request, parseRunSnapshot);
}

export function directDecrypt(request: DirectDecryptRequest): Promise<DirectDecryptResponse> {
  return postJson("/decrypt/direct", request, parseDirectDecryptResponse);
}

export function getRun(runId: string): Promise<RunSnapshot> {
  return requestJson(`/runs/${encodeURIComponent(runId)}`, undefined, parseRunSnapshot);
}

export function cancelRun(runId: string): Promise<RunSnapshot> {
  return requestJson(`/runs/${encodeURIComponent(runId)}/cancel`, { method: "POST" }, parseRunSnapshot);
}

export function deleteRun(runId: string): Promise<void> {
  return requestJson(`/runs/${encodeURIComponent(runId)}`, { method: "DELETE" }, (value) => {
    if (!isRecord(value) || value.deleted !== true) throw new Error("The API returned an invalid delete response.");
  });
}

export function getRunHistory(): Promise<RunHistoryResponse> {
  return requestJson("/runs", undefined, (value) => {
    if (!isRecord(value) || !Array.isArray(value.runs)) throw new Error("The API returned invalid run history.");
    return { runs: value.runs.map(parseRunSnapshot) };
  });
}

export function notifySessionDisconnect(): void {
  const body = new Blob(["{}"], { type: "application/json" });
  if (!navigator.sendBeacon || !navigator.sendBeacon(`${API_ROOT}/session/disconnect`, body)) {
    void fetch(`${API_ROOT}/session/disconnect`, { method: "POST", headers: { "Content-Type": "application/json" }, body: "{}", keepalive: true });
  }
}
