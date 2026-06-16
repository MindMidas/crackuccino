import { useState } from "react";
import type { DictionaryDetail, DictionaryInfo } from "../types/cipher";
import type { SectionMessage } from "../utils/messages";
import { sectionError } from "../utils/messages";
import { FieldLabel } from "./FieldLabel";
import { MessageBanner } from "./MessageBanner";
import { Panel } from "./Panel";

const WORD_PATTERN = /^[A-Za-z]+(?:'[A-Za-z]+)*$/;

interface DictionaryManagerProps {
  mode: "dictionaries" | "upload";
  dictionaries: DictionaryInfo[];
  selectedDictionary: DictionaryDetail | null;
  loading: boolean;
  dictionaryMessage: SectionMessage | null;
  uploadMessage: SectionMessage | null;
  createMessage: SectionMessage | null;
  onSelectDictionary: (dictionaryId: string) => Promise<void>;
  onUploadDictionary: (file: File) => Promise<void>;
  onCreateDictionary: (name: string, words: string[]) => Promise<void>;
}

export function DictionaryManager({
  mode,
  dictionaries,
  selectedDictionary,
  loading,
  dictionaryMessage,
  uploadMessage,
  createMessage,
  onSelectDictionary,
  onUploadDictionary,
  onCreateDictionary,
}: DictionaryManagerProps) {
  return mode === "upload" ? (
    <section className="upload-view-grid" aria-label="Dictionary upload">
      <DictionaryUploadPanel loading={loading} message={uploadMessage} onUploadDictionary={onUploadDictionary} />
      <DictionaryCreatePanel loading={loading} message={createMessage} onCreateDictionary={onCreateDictionary} />
    </section>
  ) : (
    <DictionaryViewerPanel
      dictionaries={dictionaries}
      selectedDictionary={selectedDictionary}
      loading={loading}
      message={dictionaryMessage}
      onSelectDictionary={onSelectDictionary}
    />
  );
}

function DictionaryViewerPanel({
  dictionaries,
  selectedDictionary,
  loading,
  message,
  onSelectDictionary,
}: {
  dictionaries: DictionaryInfo[];
  selectedDictionary: DictionaryDetail | null;
  loading: boolean;
  message: SectionMessage | null;
  onSelectDictionary: (dictionaryId: string) => Promise<void>;
}) {
  return (
    <section className="dictionary-view-grid" aria-label="Dictionaries">
      <Panel eyebrow="Dictionaries" title="Available dictionaries" className="dictionary-list-panel">
        <div className="dictionary-list">
          {dictionaries.map((dictionary) => (
            <button
              className={selectedDictionary?.id === dictionary.id ? "active" : ""}
              key={dictionary.id}
              onClick={() => { void onSelectDictionary(dictionary.id); }}
              type="button"
            >
              <span>
                <strong>{dictionary.label}</strong>
                <em>{dictionary.source}</em>
              </span>
              <small>{dictionary.wordCount.toLocaleString()} words</small>
            </button>
          ))}
          {dictionaries.length === 0 && <div className="empty-state">No dictionaries are available.</div>}
        </div>
      </Panel>

      <Panel
        eyebrow={selectedDictionary?.source ?? "Preview"}
        title={selectedDictionary?.label ?? "Select a dictionary"}
        actions={selectedDictionary ? <span className="status-dot">{selectedDictionary.wordCount.toLocaleString()} words</span> : undefined}
        className="dictionary-preview-panel"
      >
        {message && <MessageBanner className="mb-3" type={message.type} title={message.title} message={message.message} />}
        {loading ? (
          <div className="empty-state">Loading dictionary...</div>
        ) : selectedDictionary ? (
          <>
            <div className="dictionary-preview">
              {selectedDictionary.words.map((word, index) => <code key={`${word}-${index}`}>{word}</code>)}
            </div>
            {selectedDictionary.truncated && (
              <p className="dictionary-note">Showing only the first 800 words</p>
            )}
          </>
        ) : (
          <div className="empty-state">Choose a dictionary to view its words.</div>
        )}
      </Panel>
    </section>
  );
}

function DictionaryUploadPanel({
  loading,
  message,
  onUploadDictionary,
}: {
  loading: boolean;
  message: SectionMessage | null;
  onUploadDictionary: (file: File) => Promise<void>;
}) {
  const [file, setFile] = useState<File | null>(null);

  const uploadFile = () => {
    if (!file || loading) return;
    void onUploadDictionary(file);
  };

  return (
    <Panel eyebrow="Upload" title="Upload a file">
      <div className="upload-form">
        {message && <MessageBanner type={message.type} title={message.title} message={message.message} />}
        <div>
          <FieldLabel htmlFor="dictionary-file" tip="Plain text, one word per line.">
            Dictionary file
          </FieldLabel>
          <input
            id="dictionary-file"
            accept=".txt,text/plain"
            className="file-input"
            onChange={(event) => setFile(event.target.files?.[0] ?? null)}
            type="file"
          />
        </div>
        <div className="format-card">
          <p className="field-label">Expected format</p>
          <ul>
            <li>UTF-8 `.txt` file only.</li>
            <li>One word per line.</li>
            <li>Only lowercase/uppercase English letters `a-z` are supported.</li>
            <li>Apostrophes are allowed inside words, like `can't`.</li>
            <li>Blank lines are ignored. Duplicate words are removed.</li>
            <li>Custom dictionaries stay in this page session only and clear on hard refresh.</li>
          </ul>
        </div>
        <button className="primary-button w-full" disabled={!file || loading} onClick={uploadFile} type="button">
          {loading ? "Uploading..." : "Upload dictionary"}
        </button>
      </div>
    </Panel>
  );
}

function DictionaryCreatePanel({
  loading,
  message,
  onCreateDictionary,
}: {
  loading: boolean;
  message: SectionMessage | null;
  onCreateDictionary: (name: string, words: string[]) => Promise<void>;
}) {
  const [name, setName] = useState("my_dictionary");
  const [draft, setDraft] = useState("");
  const [words, setWords] = useState<string[]>([]);
  const [validationMessage, setValidationMessage] = useState<SectionMessage | null>(null);

  const activeMessage = validationMessage ?? message;

  const addWord = () => {
    const word = draft.trim().toLowerCase();
    if (!word) return;
    if (!WORD_PATTERN.test(word)) {
      setValidationMessage(sectionError("Invalid word", "Words must use only a-z letters, with optional apostrophes inside."));
      return;
    }
    if (words.includes(word)) {
      setValidationMessage(sectionError("Duplicate word", "That word is already in your list."));
      return;
    }
    setWords((current) => [...current, word]);
    setDraft("");
    setValidationMessage(null);
  };

  const removeWord = (word: string) => {
    setWords((current) => current.filter((item) => item !== word));
  };

  const downloadDictionary = () => {
    if (words.length === 0) return;
    const label = name.trim() || "my_dictionary";
    const blob = new Blob([`${words.join("\n")}\n`], { type: "text/plain;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `${label.replace(/[^\w.-]+/g, "_")}.txt`;
    link.click();
    URL.revokeObjectURL(url);
  };

  return (
    <Panel eyebrow="Create" title="Build a dictionary">
      <form
        className="upload-form"
        onSubmit={(event) => {
          event.preventDefault();
          if (words.length === 0) {
            setValidationMessage(sectionError("Dictionary empty", "Add at least one word before saving."));
            return;
          }
          setValidationMessage(null);
          void onCreateDictionary(name.trim() || "my_dictionary", words);
        }}
      >
        {activeMessage && <MessageBanner type={activeMessage.type} title={activeMessage.title} message={activeMessage.message} />}
        <div>
          <FieldLabel htmlFor="dictionary-name" tip="Label for your custom list.">
            Dictionary name
          </FieldLabel>
          <input
            id="dictionary-name"
            className="text-input"
            onChange={(event) => setName(event.target.value)}
            placeholder="my_dictionary"
            type="text"
            value={name}
          />
        </div>
        <div>
          <FieldLabel htmlFor="dictionary-word" tip="Type a valid dictionary word.">
            Add a word
          </FieldLabel>
          <div className="create-word-row">
            <input
              id="dictionary-word"
              className="text-input"
              onChange={(event) => setDraft(event.target.value)}
              onKeyDown={(event) => {
                if (event.key === "Enter") {
                  event.preventDefault();
                  addWord();
                }
              }}
              placeholder="brew"
              type="text"
              value={draft}
            />
            <button className="secondary-button" onClick={addWord} type="button">Add</button>
          </div>
        </div>
        {words.length > 0 && (
          <div className="word-chip-list">
            {words.map((word) => (
              <span className="word-chip" key={word}>
                <code>{word}</code>
                <button aria-label={`Remove ${word}`} onClick={() => removeWord(word)} type="button">×</button>
              </span>
            ))}
          </div>
        )}
        <div className="create-actions">
          <button className="primary-button" disabled={words.length === 0 || loading} type="submit">
            {loading ? "Saving..." : "Save dictionary"}
          </button>
          <button className="secondary-button" disabled={words.length === 0} onClick={downloadDictionary} type="button">
            Download .txt
          </button>
        </div>
        <p className="dictionary-note">Saved dictionaries are validated and kept for this browser session only.</p>
      </form>
    </Panel>
  );
}
