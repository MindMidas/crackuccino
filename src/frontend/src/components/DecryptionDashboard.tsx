import { useEffect, useMemo, useRef, useState } from "react";
import type {
  CustomDictionary,
  DecryptRequest,
  DictionaryInfo,
  DictionaryName,
  DirectDecryptRequest,
  DirectDecryptResponse,
} from "../types/cipher";
import type { SectionMessage } from "../utils/messages";
import { FieldLabel } from "./FieldLabel";
import { InfoTip } from "./InfoTip";
import { MessageBanner } from "./MessageBanner";
import { Panel } from "./Panel";
import { SelectField, type SelectOption } from "./SelectField";
import { ShuffleEditor } from "./ShuffleEditor";

const MAX_BRUTE_FORCE_PERMUTATIONS = 1_000_000_000;

interface DecryptionDashboardProps {
  ciphertext: string;
  plaintext: string;
  inputDictionary: string;
  encryptionDictionary: string;
  dictionaries: DictionaryInfo[];
  customDictionaries: CustomDictionary[];
  loading: boolean;
  directLoading: boolean;
  directResult: DirectDecryptResponse | null;
  message: SectionMessage | null;
  restoreKey: string | null;
  restoredDictionary: DictionaryName | null;
  restoredRanks: number | null;
  restoredDepth: number | null;
  resetKey: number;
  onDecrypt: (request: DecryptRequest) => Promise<void>;
  onDirectDecrypt: (request: DirectDecryptRequest) => Promise<void>;
}

function uniqueLetters(value: string): string {
  return [...new Set([...value.toLowerCase()].filter((char) => char >= "a" && char <= "z"))].join("");
}

function factorial(value: number): number {
  let result = 1;
  for (let number = 2; number <= value; number += 1) result *= number;
  return result;
}

function directMappingCanRun(cipherLetters: string, plainMapping: string): boolean {
  return cipherLetters.length > 0
    && plainMapping.length > 0
    && new Set(cipherLetters).size === cipherLetters.length
    && new Set(plainMapping).size === plainMapping.length;
}

export function DecryptionDashboard({
  ciphertext,
  plaintext,
  dictionaries,
  customDictionaries,
  loading,
  directLoading,
  directResult,
  message,
  restoreKey,
  restoredDictionary,
  restoredRanks,
  restoredDepth,
  resetKey,
  onDecrypt,
  onDirectDecrypt,
}: DecryptionDashboardProps) {
  const [input, setInput] = useState(ciphertext);
  const [mode, setMode] = useState<"brute-force" | "known-mapping">("brute-force");
  const [dictionary, setDictionary] = useState<DictionaryName>("american_english_dictionary");
  const [ranks, setRanks] = useState(4);
  const [depth, setDepth] = useState("auto");
  const [knownDecryptionDictionary, setKnownDecryptionDictionary] = useState("");
  const prevCipherLettersRef = useRef("");
  const cipherLetters = uniqueLetters(input);
  const dictionaryOptions: SelectOption[] = useMemo(
    () => dictionaries.length > 0
      ? dictionaries.map((item) => [item.id, `${item.label} (${item.wordCount.toLocaleString()})`])
      : [["american_english_dictionary", "American English"]],
    [dictionaries],
  );
  const uniqueCount = cipherLetters.length;
  const permutations = factorial(uniqueCount);
  const workloadAllowed = permutations <= MAX_BRUTE_FORCE_PERMUTATIONS;
  const knownMappingValid = directMappingCanRun(cipherLetters, knownDecryptionDictionary);
  const currentDirectResult = directResult?.ciphertext === input.trim()
    && directResult.inputDictionary === knownDecryptionDictionary
    && directResult.encryptionDictionary === cipherLetters
    ? directResult
    : null;

  useEffect(() => {
    if (ciphertext) setInput(ciphertext);
  }, [ciphertext]);
  useEffect(() => {
    if (!restoreKey) return;
    setInput(ciphertext);
    setMode("brute-force");
    setKnownDecryptionDictionary("");
    setDictionary(restoredDictionary ?? "american_english_dictionary");
    setRanks(restoredRanks ?? 4);
    setDepth(restoredDepth === null ? "auto" : String(restoredDepth));
  }, [ciphertext, restoreKey, restoredDepth, restoredDictionary, restoredRanks]);
  useEffect(() => {
    setInput("");
    setMode("brute-force");
    setDictionary("american_english_dictionary");
    setRanks(4);
    setDepth("auto");
    setKnownDecryptionDictionary("");
  }, [resetKey]);
  useEffect(() => {
    if (prevCipherLettersRef.current !== cipherLetters) {
      setKnownDecryptionDictionary("");
    }
    prevCipherLettersRef.current = cipherLetters;
  }, [cipherLetters]);
  useEffect(() => {
    if (dictionaryOptions.some(([value]) => value === dictionary)) return;
    setDictionary(dictionaryOptions[0]?.[0] ?? "american_english_dictionary");
  }, [dictionary, dictionaryOptions]);

  const canBruteForce = input.trim().length > 0 && workloadAllowed && !loading;
  const canDirectDecrypt = input.trim().length > 0 && knownMappingValid && !directLoading;

  return (
    <Panel
      eyebrow="02 / Decrypt"
      title="Configure decryption"
      actions={(
        <div className="decrypt-mode-actions">
          <div className="segmented-control decrypt-mode-toggle" role="group" aria-label="Decrypt mode">
            <button className={`segmented-option${mode === "brute-force" ? " active" : ""}`} onClick={() => setMode("brute-force")} type="button">Brute-force search</button>
            <button className={`segmented-option${mode === "known-mapping" ? " active" : ""}`} onClick={() => setMode("known-mapping")} type="button">Known mapping</button>
          </div>
          <InfoTip
            ariaLabel="About decrypt modes"
            text="Brute-force search runs serially or across MPI ranks. Known mapping directly decrypts with the plaintext letter order you provide."
          />
        </div>
      )}
      className="control-panel"
      bodyClassName="setup-panel-body"
    >
      <form
        className="setup-form"
        onSubmit={(event) => {
          event.preventDefault();
          if (mode === "known-mapping") {
            if (canDirectDecrypt) void onDirectDecrypt({
              ciphertext: input.trim(),
              inputDictionary: knownDecryptionDictionary,
              encryptionDictionary: cipherLetters,
            });
            return;
          }
          if (!canBruteForce) return;
          const customDictionary = customDictionaries.find((item) => item.id === dictionary);
          void onDecrypt({
            ciphertext: input.trim(),
            plaintext,
            dictionary,
            ranks,
            depth: depth === "auto" ? null : Number(depth),
            ...(customDictionary ? {
              customDictionary: {
                id: customDictionary.id,
                label: customDictionary.label,
                content: customDictionary.content,
              },
            } : {}),
          });
        }}
      >
        {message && <MessageBanner type={message.type} title={message.title} message={message.message} />}
        <div>
          <FieldLabel htmlFor="ciphertext" tip="The scrambled text to decrypt.">Ciphertext</FieldLabel>
          <textarea id="ciphertext" className="text-area text-area-code" value={input} maxLength={240} onChange={(event) => setInput(event.target.value)} rows={3} />
        </div>
        <div className="decrypt-action-stack">
          {mode === "brute-force" ? (
            <>
              <div className={`workload-estimate ${workloadAllowed ? "" : "workload-estimate-blocked"}`}>
                <span>{uniqueCount} unique letters</span>
                <strong>{permutations.toLocaleString()} permutations</strong>
                <small>{workloadAllowed ? "Within the interactive safety limit." : "This would roast our tiny compute budget. Try fewer unique letters or use a known mapping."}</small>
              </div>
              <div className="grid gap-3 sm:grid-cols-3">
                <SelectField label="Dictionary" tip="Words for scoring good guesses." value={dictionary} onChange={setDictionary} options={dictionaryOptions} />
                <SelectField label="Workers" tip="Use serial search or split the work across MPI ranks." value={String(ranks)} onChange={(value) => setRanks(Number(value))} options={[[1, "Serial"], [2, "2 ranks"], [4, "4 ranks"], [6, "6 ranks"], [8, "8 ranks"]].map(([value, label]): SelectOption => [String(value), String(label)])} />
                <SelectField label="Prefix depth" tip="How MPI work is split into assigned prefix tasks." value={depth} onChange={setDepth} disabled={ranks === 1} options={[["auto", "Auto"], ["1", "Depth 1"], ["2", "Depth 2"], ["3", "Depth 3"]]} />
              </div>
              <button className="primary-button w-full" type="submit" disabled={!canBruteForce}>
                {loading ? "Running search..." : ranks === 1 ? "Decrypt serially" : "Decrypt across ranks"}
              </button>
            </>
          ) : (
            <>
              <div className="mapping-card">
                <div className="mapping-card-header">
                  <p className="field-label">Known letter mapping</p>
                  <span className="status-dot">{cipherLetters.length} letters</span>
                </div>
                <p className="field-hint">Top row is from the ciphertext. Drag the bottom row to say what each letter decrypts to.</p>
                {cipherLetters ? (
                  <ShuffleEditor
                    inputDictionary={cipherLetters}
                    mapping={knownDecryptionDictionary}
                    allowLengthMismatch
                    originalLabel="Cipher"
                    mappingLabel="Plain"
                    onMappingChange={setKnownDecryptionDictionary}
                  />
                ) : <div className="empty-state mapping-empty-state">Enter ciphertext to build the mapping tool.</div>}
                <label>
                  <span className="field-label">Decryption mapping</span>
                  <input
                    className="text-input text-area-code"
                    value={knownDecryptionDictionary}
                    maxLength={26}
                    disabled={!cipherLetters}
                    onChange={(event) => {
                      setKnownDecryptionDictionary(uniqueLetters(event.target.value));
                    }}
                    placeholder="Paste plaintext or letter order"
                  />
                </label>
              </div>
              <p className="field-hint">
                {knownMappingValid
                  ? "Mapping will decrypt matching positions. Extra letters are ignored."
                  : "Enter ciphertext and at least one plaintext mapping letter."}
              </p>
              <div className={`direct-result${currentDirectResult ? "" : " direct-result-empty"}`}>
                <p className="field-label">Direct plaintext output</p>
                <p className="direct-result-value">{currentDirectResult?.plaintext ?? "—"}</p>
              </div>
              <button className="primary-button w-full" type="submit" disabled={!canDirectDecrypt}>
                {directLoading ? "Decrypting..." : "Decrypt with known mapping"}
              </button>
            </>
          )}
        </div>
      </form>
    </Panel>
  );
}
