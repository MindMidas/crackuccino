export type DictionaryName = string;
export type RankState = "complete" | "running" | "queued" | "canceled";
export type RunState = "queued" | "running" | "complete" | "failed" | "canceled";
export type DictionarySource = "builtin" | "custom";

export interface EncryptRequest {
  plaintext: string;
  encryptionDictionary?: string;
}

export interface EncryptResponse {
  plaintext: string;
  inputDictionary: string;
  encryptionDictionary: string;
  ciphertext: string;
}

export interface DecryptRequest {
  ciphertext: string;
  plaintext: string;
  dictionary: DictionaryName;
  ranks: number;
  depth: number | null;
  customDictionary?: CustomDictionaryPayload;
}

export interface DirectDecryptRequest {
  ciphertext: string;
  inputDictionary: string;
  encryptionDictionary: string;
}

export interface DirectDecryptResponse extends DirectDecryptRequest {
  plaintext: string;
}

export interface MpiRankStatus {
  rank: number;
  state: RankState;
  permutations: number;
  hits: number;
  prefix: string;
  samplePermutation: string;
  permutationTrace: PermutationTraceEntry[];
  completedTasks: number;
  assignedTasks: number;
  assignedPermutations: number;
}

export interface PermutationTraceEntry {
  permutation: string;
  visited: number;
  prefix: string;
}

export interface DecryptionResult {
  rank: number;
  permutation: string;
  plaintext: string;
}

export interface BenchmarkStats {
  ranks: number;
  uniqueLetters: number;
  depth: number;
  expectedPermutations: number;
  visitedPermutations: number;
  validHits: number;
  runtimeSeconds: number;
  serialEstimateSeconds: number;
  speedup: number;
  efficiencyPercent: number;
  progressPercent: number;
}

export interface RunSnapshot {
  id: string;
  state: RunState;
  createdAt: number;
  completedAt: number | null;
  request: DecryptRequest;
  results: DecryptionResult[];
  rankStatuses: MpiRankStatus[];
  stats: BenchmarkStats;
  error: string | null;
}

export interface RunHistoryResponse {
  runs: RunSnapshot[];
}

export interface DictionaryInfo {
  id: DictionaryName;
  label: string;
  source: DictionarySource;
  wordCount: number;
}

export interface DictionaryDetail extends DictionaryInfo {
  words: string[];
  truncated: boolean;
}

export interface DictionaryListResponse {
  dictionaries: DictionaryInfo[];
}

export interface DictionaryUploadRequest {
  filename: string;
  content: string;
}

export interface DictionaryUploadResponse {
  dictionary: CustomDictionary;
}

export interface CustomDictionaryPayload {
  id: string;
  label: string;
  content: string;
}

export interface CustomDictionary extends DictionaryDetail {
  source: "custom";
  content: string;
}
