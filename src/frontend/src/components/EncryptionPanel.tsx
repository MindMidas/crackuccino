import { useEffect, useMemo, useState } from "react";
import type { EncryptRequest, EncryptResponse } from "../types/cipher";
import type { SectionMessage } from "../utils/messages";
import { sectionError } from "../utils/messages";
import { FieldLabel } from "./FieldLabel";
import { InfoTip } from "./InfoTip";
import { MessageBanner } from "./MessageBanner";
import { Panel } from "./Panel";
import { ShuffleEditor } from "./ShuffleEditor";

interface EncryptionPanelProps {
  loading: boolean;
  result: EncryptResponse | null;
  message: SectionMessage | null;
  restoreKey: string | null;
  restoredPlaintext: string | null;
  resetKey: number;
  onEncrypt: (request: EncryptRequest) => Promise<void>;
}

export function EncryptionPanel({ loading, result, message, restoreKey, restoredPlaintext, resetKey, onEncrypt }: EncryptionPanelProps) {
  const [plaintext, setPlaintext] = useState("brew cappuccino");
  const [customShuffle, setCustomShuffle] = useState(false);
  const [mapping, setMapping] = useState("");
  const [localMessage, setLocalMessage] = useState<SectionMessage | null>(null);
  const inputDictionary = useMemo(() => buildInputDictionary(plaintext), [plaintext]);
  const fallbackMapping = useMemo(() => inputDictionary.split("").reverse().join(""), [inputDictionary]);
  const activeMessage = localMessage ?? message;
  const canSubmit = plaintext.trim().length > 0 && plaintext.length <= 240 && !loading;

  useEffect(() => {
    if (customShuffle) setMapping(fallbackMapping);
  }, [customShuffle, fallbackMapping]);
  useEffect(() => {
    if (!restoreKey) return;
    setPlaintext(restoredPlaintext ?? "");
    setCustomShuffle(false);
    setMapping("");
    setLocalMessage(null);
  }, [restoreKey, restoredPlaintext]);
  useEffect(() => {
    setPlaintext("brew cappuccino");
    setCustomShuffle(false);
    setMapping("");
    setLocalMessage(null);
  }, [resetKey]);

  const enableCustomShuffle = (enabled: boolean) => {
    setCustomShuffle(enabled);
    setLocalMessage(null);
    if (enabled && !mapping) setMapping(fallbackMapping);
  };

  return (
    <Panel eyebrow="01 / Prepare" title="Encrypt plaintext" className="control-panel" bodyClassName="setup-panel-body">
      <form
        className="setup-form"
        onSubmit={(event) => {
          event.preventDefault();
          if (!canSubmit) return;

          const request: EncryptRequest = { plaintext: plaintext.trim() };
          if (customShuffle) {
            const validMapping = validateCustomMapping(mapping, inputDictionary);
            if (!validMapping.ok) {
              setLocalMessage(sectionError("Custom mapping invalid", validMapping.message));
              return;
            }
            request.encryptionDictionary = validMapping.value;
          }

          setLocalMessage(null);
          void onEncrypt(request);
        }}
      >
        {activeMessage && <MessageBanner type={activeMessage.type} title={activeMessage.title} message={activeMessage.message} />}
        <div>
          <FieldLabel htmlFor="plaintext" tip="The message you want encrypted.">
            Plaintext message
          </FieldLabel>
          <textarea
            id="plaintext"
            className="text-area"
            value={plaintext}
            maxLength={240}
            onChange={(event) => {
              setPlaintext(event.target.value);
              setLocalMessage(null);
            }}
            placeholder="Enter a short plaintext message"
            rows={3}
          />
          <p className="field-hint">{plaintext.length}/240 characters</p>
        </div>
        <div className="encrypt-bottom-stack">
          <div className="mapping-card">
            <div className="mapping-card-header">
              <p className="field-label field-label-row">
                <span>Letter shuffle</span>
                <InfoTip ariaLabel="About letter shuffle" text="Random shuffle or drag letters." />
              </p>
              <div className="segmented-control" role="group" aria-label="Letter shuffle mode">
                <button
                  className={`segmented-option${customShuffle ? "" : " active"}`}
                  onClick={() => enableCustomShuffle(false)}
                  type="button"
                >
                  Random
                </button>
                <button
                  className={`segmented-option${customShuffle ? " active" : ""}`}
                  onClick={() => enableCustomShuffle(true)}
                  type="button"
                >
                  Custom
                </button>
              </div>
            </div>
            {customShuffle && (
              <p className="field-hint">
                {inputDictionary ? "Hold and drag letters to reorder." : "Type plaintext letters to build a shuffle."}
              </p>
            )}
            {customShuffle && inputDictionary && (
              <ShuffleEditor
                inputDictionary={inputDictionary}
                mapping={mapping}
                onMappingChange={(value) => {
                  setMapping(value);
                  setLocalMessage(null);
                }}
              />
            )}
          </div>
          <div className="dictionary-grid">
            <Dictionary label="Input dictionary" tip="Unique letters in your message." value={result?.inputDictionary ?? "—"} />
            <Dictionary label="Shuffled mapping" tip="How letters map to ciphertext." value={result?.encryptionDictionary ?? "—"} accent />
          </div>
        </div>
        <button className="primary-button w-full" type="submit" disabled={!canSubmit}>
          {loading ? "Encrypting..." : "Encrypt message"}
        </button>
      </form>
    </Panel>
  );
}

function buildInputDictionary(text: string): string {
  const seen = new Set<string>();
  const letters: string[] = [];
  for (const char of text.toLowerCase()) {
    if (char < "a" || char > "z" || seen.has(char)) continue;
    seen.add(char);
    letters.push(char);
  }
  return letters.join("");
}

function validateCustomMapping(mapping: string, inputDictionary: string): { ok: true; value: string } | { ok: false; message: string } {
  const value = mapping.trim().toLowerCase();
  if (!inputDictionary) return { ok: false, message: "Plaintext needs at least one letter before custom shuffle can run." };
  if (!/^[a-z]+$/.test(value)) return { ok: false, message: "Mapping must use only letters a-z." };
  if (value.length !== inputDictionary.length) return { ok: false, message: `Mapping must be ${inputDictionary.length} letters long.` };
  if ([...value].sort().join("") !== [...inputDictionary].sort().join("")) {
    return { ok: false, message: `Mapping must use exactly these letters once: ${inputDictionary}.` };
  }
  if (inputDictionary.length > 1 && value === inputDictionary) {
    return { ok: false, message: "Mapping must change the letter order." };
  }
  return { ok: true, value };
}

function Dictionary({
  label,
  value,
  accent = false,
  tip,
}: {
  label: string;
  value: string;
  accent?: boolean;
  tip?: string;
}) {
  return (
    <div className="dictionary-card">
      <FieldLabel tip={tip}>{label}</FieldLabel>
      <p className={`dictionary-card-value${accent ? " accent" : ""}`}>{value}</p>
    </div>
  );
}
