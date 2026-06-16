import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { cancelRun, deleteRun, directDecrypt, encryptText, getDictionaries, getDictionary, getRun, getRunHistory, isApiNotFound, notifySessionDisconnect, startRun, uploadDictionary } from "./api/cipherApi";
import { DecryptionDashboard } from "./components/DecryptionDashboard";
import { DictionaryManager } from "./components/DictionaryManager";
import { EncryptionPanel } from "./components/EncryptionPanel";
import { MessageBanner } from "./components/MessageBanner";
import { MpiRankGrid } from "./components/MpiRankGrid";
import { PerformancePanel } from "./components/PerformancePanel";
import { ResultsPanel } from "./components/ResultsPanel";
import { ReportPanel } from "./components/ReportPanel";
import { Sidebar, type AppView } from "./components/Sidebar";
import { MobileCopyrightFooter } from "./components/MobileCopyrightFooter";
import { SidebarCopyright } from "./components/SidebarCopyright";
import { SidebarReference } from "./components/SidebarReference";
import { SidebarShell } from "./components/SidebarShell";
import type { CustomDictionary, DecryptRequest, DictionaryDetail, DictionaryInfo, DirectDecryptRequest, DirectDecryptResponse, EncryptRequest, EncryptResponse, RunSnapshot } from "./types/cipher";
import { errorMessage, sectionError, sectionSuccess, type SectionKey, type SectionMessage } from "./utils/messages";

function requestsMatch(previous: DecryptRequest, next: DecryptRequest): boolean {
  return previous.ciphertext.trim() === next.ciphertext.trim()
    && previous.plaintext.trim() === next.plaintext.trim()
    && previous.dictionary === next.dictionary
    && previous.ranks === next.ranks
    && (previous.depth ?? null) === (next.depth ?? null);
}

function uniqueLetters(value: string): string {
  const seen = new Set<string>();
  const letters: string[] = [];
  for (const char of value.toLowerCase()) {
    if (char < "a" || char > "z" || seen.has(char)) continue;
    seen.add(char);
    letters.push(char);
  }
  return letters.join("");
}

function restoreEncryptResult(request: DecryptRequest): EncryptResponse | null {
  const plaintext = request.plaintext.trim();
  const ciphertext = request.ciphertext.trim();
  if (!plaintext || !ciphertext) return null;

  const inputDictionary = uniqueLetters(plaintext);
  const encryptedByPlain = new Map<string, string>();
  const length = Math.min(plaintext.length, ciphertext.length);
  for (let index = 0; index < length; index += 1) {
    const plain = plaintext[index]?.toLowerCase() ?? "";
    const cipher = ciphertext[index]?.toLowerCase() ?? "";
    if (plain < "a" || plain > "z" || cipher < "a" || cipher > "z") continue;
    if (!encryptedByPlain.has(plain)) encryptedByPlain.set(plain, cipher);
  }

  return {
    plaintext,
    ciphertext,
    inputDictionary,
    encryptionDictionary: [...inputDictionary].map((letter) => encryptedByPlain.get(letter) ?? "").join(""),
  };
}

export default function App() {
  const [view, setView] = useState<AppView>("workspace");
  const [encryptResult, setEncryptResult] = useState<EncryptResponse | null>(null);
  const [activeRun, setActiveRun] = useState<RunSnapshot | null>(null);
  const [history, setHistory] = useState<RunSnapshot[]>([]);
  const [dictionaries, setDictionaries] = useState<DictionaryInfo[]>([]);
  const [customDictionaries, setCustomDictionaries] = useState<CustomDictionary[]>([]);
  const [selectedDictionary, setSelectedDictionary] = useState<DictionaryDetail | null>(null);
  const [dictionaryLoading, setDictionaryLoading] = useState(false);
  const [messages, setMessages] = useState<Partial<Record<SectionKey, SectionMessage>>>({});
  const [encrypting, setEncrypting] = useState(false);
  const [directDecrypting, setDirectDecrypting] = useState(false);
  const [directDecryptResult, setDirectDecryptResult] = useState<DirectDecryptResponse | null>(null);
  const mainRef = useRef<HTMLElement>(null);
  const [runActionPending, setRunActionPending] = useState(false);
  const [runStarting, setRunStarting] = useState(false);
  const [workspaceResetKey, setWorkspaceResetKey] = useState(0);

  const setMessage = useCallback((section: SectionKey, message: SectionMessage | null) => {
    setMessages((current) => {
      const next = { ...current };
      if (message) next[section] = message;
      else delete next[section];
      return next;
    });
  }, []);

  const clearMessages = useCallback((...sections: SectionKey[]) => {
    setMessages((current) => {
      const next = { ...current };
      sections.forEach((section) => delete next[section]);
      return next;
    });
  }, []);

  const mergeRunIntoHistory = useCallback((run: RunSnapshot) => {
    setHistory((current) => [run, ...current.filter((item) => item.id !== run.id)]);
  }, []);

  const refreshHistory = useCallback(async () => {
    try {
      setHistory((await getRunHistory()).runs);
    } catch {
      // History is secondary; active request errors remain visible separately.
    }
  }, []);

  const refreshDictionaries = useCallback(async () => {
    const response = await getDictionaries();
    setDictionaries(response.dictionaries);
    return response.dictionaries;
  }, []);

  const availableDictionaries = useMemo(
    () => [...dictionaries, ...customDictionaries],
    [customDictionaries, dictionaries],
  );
  const activeRunId = activeRun?.id;
  const activeRunState = activeRun?.state;

  const selectDictionary = useCallback(async (dictionaryId: string) => {
    setDictionaryLoading(true);
    clearMessages("dictionary");
    try {
      const customDictionary = customDictionaries.find((item) => item.id === dictionaryId);
      setSelectedDictionary(customDictionary ?? await getDictionary(dictionaryId));
    } catch (reason) {
      setMessage("dictionary", sectionError("Dictionary load failed", errorMessage(reason, "Could not load dictionary.")));
    } finally {
      setDictionaryLoading(false);
    }
  }, [clearMessages, customDictionaries, setMessage]);

  useEffect(() => { void refreshHistory(); }, [refreshHistory]);
  useEffect(() => {
    const disconnect = () => notifySessionDisconnect();
    window.addEventListener("pagehide", disconnect);
    return () => window.removeEventListener("pagehide", disconnect);
  }, []);
  useEffect(() => {
    void refreshDictionaries().then((items) => {
      if (items[0]) void getDictionary(items[0].id).then(setSelectedDictionary);
    }).catch(() => {
      // Dictionary list errors surface when the user opens the dictionary view.
    });
  }, [refreshDictionaries]);

  useEffect(() => {
    if (!activeRunId || (activeRunState !== "queued" && activeRunState !== "running")) return;
    const timer = window.setInterval(() => {
      void getRun(activeRunId).then((run) => {
        setActiveRun(run);
        mergeRunIntoHistory(run);
        if (run.state === "canceled") {
          setMessage("run", sectionSuccess("MPI run canceled", run.error ?? "Run stopped cleanly."));
        } else if (run.error) {
          setMessage("run", sectionError("MPI run failed", run.error));
        } else {
          clearMessages("run");
        }
      }).catch((reason: unknown) => {
        if (isApiNotFound(reason)) {
          setActiveRun(null);
          void refreshHistory();
          setMessage("run", sectionError("Run unavailable", "This run no longer exists in the current browser session."));
          return;
        }
        setMessage("run", sectionError("Progress update failed", errorMessage(reason, "Could not refresh run progress.")));
      });
    }, 250);
    return () => window.clearInterval(timer);
  }, [activeRunId, activeRunState, clearMessages, mergeRunIntoHistory, refreshHistory, setMessage]);

  const runEncrypt = useCallback(async (request: EncryptRequest) => {
    setEncrypting(true);
    clearMessages("encrypt");
    try {
      setEncryptResult(await encryptText(request));
      setActiveRun(null);
      clearMessages("run");
      setView("workspace");
    } catch (reason) {
      setMessage("encrypt", sectionError("Encryption failed", errorMessage(reason, "Encryption failed.")));
    } finally {
      setEncrypting(false);
    }
  }, [clearMessages, setMessage]);

  const runDecrypt = useCallback(async (request: DecryptRequest) => {
    clearMessages("decrypt");

    const matchingLocalRun = history.find((run) => requestsMatch(run.request, request));
    if (matchingLocalRun) {
      if (activeRun?.id === matchingLocalRun.id) {
        return;
      }
      setActiveRun(matchingLocalRun);
      setView("workspace");
      clearMessages("run");
      return;
    }

    setRunStarting(true);
    clearMessages("run");
    try {
      const latestHistory = (await getRunHistory()).runs;
      setHistory(latestHistory);
      const matchingRun = latestHistory.find((run) => requestsMatch(run.request, request));
      if (matchingRun) {
        if (activeRun?.id !== matchingRun.id) {
          setActiveRun(matchingRun);
        }
        setView("workspace");
        clearMessages("run");
        return;
      }
      const run = await startRun(request);
      setActiveRun(run);
      mergeRunIntoHistory(run);
      if (run.error) {
        setMessage("run", sectionError("MPI run failed", run.error));
      }
      setView("workspace");
    } catch (reason) {
      setMessage("decrypt", sectionError("MPI start failed", errorMessage(reason, "Could not start MPI run.")));
    } finally {
      setRunStarting(false);
    }
  }, [activeRun?.id, clearMessages, history, mergeRunIntoHistory, setMessage]);

  const runDirectDecrypt = useCallback(async (request: DirectDecryptRequest) => {
    setDirectDecrypting(true);
    clearMessages("decrypt");
    try {
      setDirectDecryptResult(await directDecrypt(request));
      setMessage("decrypt", sectionSuccess("Known mapping decrypted", "The mapping was inverted directly without starting MPI."));
    } catch (reason) {
      setMessage("decrypt", sectionError("Direct decrypt failed", errorMessage(reason, "Could not decrypt with this mapping.")));
    } finally {
      setDirectDecrypting(false);
    }
  }, [clearMessages, setMessage]);

  const selectRun = useCallback((run: RunSnapshot) => {
    setActiveRun(run);
    setView("workspace");
    clearMessages("encrypt", "decrypt");
    if (run.error) {
      setMessage("run", sectionError("MPI run failed", run.error));
    } else {
      clearMessages("run");
    }
  }, [clearMessages, setMessage]);

  const startNewRun = useCallback(() => {
    setActiveRun(null);
    setEncryptResult(null);
    setDirectDecryptResult(null);
    setWorkspaceResetKey((current) => current + 1);
    setView("workspace");
    clearMessages("encrypt", "decrypt", "run");
  }, [clearMessages]);

  const handleCancelRun = useCallback(async () => {
    if (!activeRun || (activeRun.state !== "queued" && activeRun.state !== "running")) return;
    setRunActionPending(true);
    clearMessages("run");
    try {
      const run = await cancelRun(activeRun.id);
      setActiveRun(run);
      mergeRunIntoHistory(run);
      setMessage("run", sectionSuccess("Cancellation requested", "The MPI process group is stopping cleanly."));
    } catch (reason) {
      setMessage("run", sectionError("Cancel failed", errorMessage(reason, "Could not cancel this run.")));
    } finally {
      setRunActionPending(false);
    }
  }, [activeRun, clearMessages, mergeRunIntoHistory, setMessage]);

  const handleDeleteRun = useCallback(async (run: RunSnapshot) => {
    setRunActionPending(true);
    try {
      await deleteRun(run.id);
      setHistory((current) => current.filter((item) => item.id !== run.id));
      if (activeRun?.id === run.id) {
        setActiveRun(null);
        clearMessages("run");
      }
    } catch (reason) {
      setMessage("run", sectionError("Delete failed", errorMessage(reason, "Could not delete this run.")));
    } finally {
      setRunActionPending(false);
    }
  }, [activeRun?.id, clearMessages, setMessage]);

  const openDictionaries = useCallback(() => {
    setView("dictionaries");
    clearMessages("upload", "create");
    void refreshDictionaries().then((items) => {
      const dictionaryId = selectedDictionary?.id ?? customDictionaries[0]?.id ?? items[0]?.id;
      if (dictionaryId) void selectDictionary(dictionaryId);
    }).catch((reason: unknown) => {
      setMessage("dictionary", sectionError("Dictionary list failed", errorMessage(reason, "Could not load dictionaries.")));
    });
  }, [clearMessages, customDictionaries, refreshDictionaries, selectDictionary, selectedDictionary?.id, setMessage]);

  const openUpload = useCallback(() => {
    setView("upload");
    clearMessages("dictionary", "upload", "create");
  }, [clearMessages]);

  const openReport = useCallback(() => {
    setView("report");
    mainRef.current?.scrollTo({ top: 0 });
  }, []);

  const closeReport = useCallback(() => {
    setView("workspace");
  }, []);

  const registerCustomDictionary = useCallback((dictionary: CustomDictionary, notice: string) => {
    setCustomDictionaries((current) => [dictionary, ...current.filter((item) => item.id !== dictionary.id)]);
    setSelectedDictionary(dictionary);
    setView("dictionaries");
    clearMessages("upload", "create");
    setMessage("dictionary", sectionSuccess("Dictionary saved", notice));
  }, [clearMessages, setMessage]);

  const handleUploadDictionary = useCallback(async (file: File) => {
    setDictionaryLoading(true);
    clearMessages("upload");
    try {
      const content = await file.text();
      const response = await uploadDictionary({ filename: file.name, content });
      registerCustomDictionary(response.dictionary, `${response.dictionary.label} is ready for this browser session.`);
    } catch (reason) {
      setMessage("upload", sectionError("Upload failed", errorMessage(reason, "Could not upload dictionary.")));
    } finally {
      setDictionaryLoading(false);
    }
  }, [clearMessages, registerCustomDictionary, setMessage]);

  const handleCreateDictionary = useCallback(async (name: string, words: string[]) => {
    setDictionaryLoading(true);
    clearMessages("create");
    try {
      const filename = `${name}.txt`;
      const content = `${words.join("\n")}\n`;
      const response = await uploadDictionary({ filename, content });
      registerCustomDictionary(response.dictionary, `${response.dictionary.label} is ready for this browser session.`);
    } catch (reason) {
      setMessage("create", sectionError("Save failed", errorMessage(reason, "Could not save dictionary.")));
    } finally {
      setDictionaryLoading(false);
    }
  }, [clearMessages, registerCustomDictionary, setMessage]);

  const running = activeRun?.state === "queued" || activeRun?.state === "running";
  const restoredRunRequest = activeRun?.request ?? null;
  const restoredRunKey = activeRun?.id ?? null;
  const restoredEncryptResult = useMemo(
    () => restoredRunRequest ? restoreEncryptResult(restoredRunRequest) : null,
    [restoredRunRequest],
  );
  const visibleEncryptResult = restoredEncryptResult ?? encryptResult;

  return (
    <div className="app-layout">
      <SidebarShell>
        <Sidebar
          view={view}
          runs={history}
          activeRun={activeRun}
          onSelectRun={selectRun}
          onDeleteRun={handleDeleteRun}
          onNewRun={startNewRun}
          onViewDictionaries={openDictionaries}
          onUploadDictionary={openUpload}
        />
        <SidebarReference onCloseReport={closeReport} onOpenReport={openReport} view={view} />
        <SidebarCopyright />
      </SidebarShell>
      <main className="app-main" ref={mainRef}>
        {view === "report" ? (
          <ReportPanel onClose={closeReport} />
        ) : view === "workspace" ? (
          <>
            <section className="setup-grid" aria-label="Run setup">
              <EncryptionPanel
                loading={encrypting}
                result={visibleEncryptResult}
                message={messages.encrypt ?? null}
                restoreKey={restoredRunKey}
                restoredPlaintext={restoredRunRequest?.plaintext ?? null}
                resetKey={workspaceResetKey}
                onEncrypt={runEncrypt}
              />
              <DecryptionDashboard
                ciphertext={restoredRunRequest?.ciphertext ?? visibleEncryptResult?.ciphertext ?? ""}
                plaintext={restoredRunRequest?.plaintext ?? visibleEncryptResult?.plaintext ?? ""}
                inputDictionary={visibleEncryptResult?.inputDictionary ?? ""}
                encryptionDictionary={visibleEncryptResult?.encryptionDictionary ?? ""}
                dictionaries={availableDictionaries}
                customDictionaries={customDictionaries}
                loading={running || runStarting}
                directLoading={directDecrypting}
                directResult={directDecryptResult}
                message={messages.decrypt ?? null}
                restoreKey={restoredRunKey}
                restoredDictionary={restoredRunRequest?.dictionary ?? null}
                restoredRanks={restoredRunRequest?.ranks ?? null}
                restoredDepth={restoredRunRequest?.depth ?? null}
                resetKey={workspaceResetKey}
                onDecrypt={runDecrypt}
                onDirectDecrypt={runDirectDecrypt}
              />
            </section>
            {messages.run && (
              <section className="dashboard-section" aria-label="Run status">
                <MessageBanner type={messages.run.type} title={messages.run.title} message={messages.run.message} />
              </section>
            )}
            {running && (
              <section className="dashboard-section run-control-bar" aria-label="Active run controls">
                <div>
                  <p className="field-label">Active MPI workload</p>
                  <p>Cancel stops the complete MPI process group and keeps the partial metrics in history.</p>
                </div>
                <button className="danger-button" disabled={runActionPending} onClick={() => void handleCancelRun()} type="button">
                  {runActionPending ? "Stopping..." : "Cancel run"}
                </button>
              </section>
            )}
            <section className="dashboard-section" aria-label="Run metrics">
              <PerformancePanel stats={activeRun?.stats ?? null} state={activeRun?.state ?? null} />
            </section>
            <section className="dashboard-section" aria-label="MPI worker progress">
              <MpiRankGrid ranks={activeRun?.rankStatuses ?? []} />
            </section>
            <section className="dashboard-section" aria-label="Decryption results">
              <ResultsPanel results={activeRun?.results ?? []} expectedPlaintext={activeRun?.request.plaintext ?? ""} state={activeRun?.state ?? null} />
            </section>
          </>
        ) : (
          <DictionaryManager
            mode={view}
            dictionaries={availableDictionaries}
            selectedDictionary={selectedDictionary}
            loading={dictionaryLoading}
            dictionaryMessage={messages.dictionary ?? null}
            uploadMessage={messages.upload ?? null}
            createMessage={messages.create ?? null}
            onSelectDictionary={selectDictionary}
            onUploadDictionary={handleUploadDictionary}
            onCreateDictionary={handleCreateDictionary}
          />
        )}
      </main>
      <MobileCopyrightFooter />
    </div>
  );
}
