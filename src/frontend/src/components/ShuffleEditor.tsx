import { useRef, useState } from "react";

function reorderLetters(letters: string[], from: number, to: number): string[] {
  if (from === to) return letters;
  const next = [...letters];
  const [moved] = next.splice(from, 1);
  if (moved === undefined) return letters;
  next.splice(to, 0, moved);
  return next;
}

function resolveInsertIndex(row: HTMLDivElement, clientX: number, slotCount: number): number {
  const chips = Array.from(row.querySelectorAll<HTMLElement>("[data-shuffle-letter]"));
  for (let index = 0; index < chips.length; index += 1) {
    const chip = chips[index];
    if (!chip) continue;
    const rect = chip.getBoundingClientRect();
    if (clientX < rect.left + rect.width / 2) return index;
  }
  return slotCount;
}

interface DragState {
  index: number;
  letter: string;
  offsetX: number;
  offsetY: number;
  width: number;
  height: number;
  x: number;
  y: number;
}

interface ShuffleEditorProps {
  inputDictionary: string;
  mapping: string;
  onMappingChange: (value: string) => void;
  allowLengthMismatch?: boolean;
  originalLabel?: string;
  mappingLabel?: string;
}

export function ShuffleEditor({
  inputDictionary,
  mapping,
  onMappingChange,
  allowLengthMismatch = false,
  originalLabel = "Original",
  mappingLabel = "Shuffle",
}: ShuffleEditorProps) {
  const rowRef = useRef<HTMLDivElement>(null);
  const dragRef = useRef<DragState | null>(null);
  const insertIndexRef = useRef<number | null>(null);
  const [drag, setDrag] = useState<DragState | null>(null);
  const [insertIndex, setInsertIndex] = useState<number | null>(null);
  const letters = allowLengthMismatch || mapping.length === inputDictionary.length ? [...mapping] : [...inputDictionary];
  const remainingEntries = drag
    ? letters.map((letter, index) => ({ letter, index })).filter((entry) => entry.index !== drag.index)
    : [];

  const finishDrag = (pointerId: number) => {
    const activeDrag = dragRef.current;
    const activeInsertIndex = insertIndexRef.current;
    if (!activeDrag) return;
    if (rowRef.current?.hasPointerCapture(pointerId)) rowRef.current.releasePointerCapture(pointerId);
    if (activeInsertIndex !== null && activeInsertIndex !== activeDrag.index) {
      onMappingChange(reorderLetters(letters, activeDrag.index, activeInsertIndex).join(""));
    }
    dragRef.current = null;
    insertIndexRef.current = null;
    setDrag(null);
    setInsertIndex(null);
  };

  const startDrag = (event: React.PointerEvent<HTMLButtonElement>, index: number, letter: string) => {
    if (event.button !== 0 || !rowRef.current) return;
    const rect = event.currentTarget.getBoundingClientRect();
    const nextDrag = {
      index,
      letter,
      offsetX: event.clientX - rect.left,
      offsetY: event.clientY - rect.top,
      width: rect.width,
      height: rect.height,
      x: rect.left,
      y: rect.top,
    };
    dragRef.current = nextDrag;
    insertIndexRef.current = index;
    setDrag(nextDrag);
    setInsertIndex(index);
    rowRef.current.setPointerCapture(event.pointerId);
  };

  const moveDrag = (event: React.PointerEvent<HTMLDivElement>) => {
    const activeDrag = dragRef.current;
    if (!activeDrag || !rowRef.current) return;
    const nextDrag = {
      ...activeDrag,
      x: event.clientX - activeDrag.offsetX,
      y: event.clientY - activeDrag.offsetY,
    };
    const nextInsertIndex = resolveInsertIndex(rowRef.current, event.clientX, letters.length - 1);
    dragRef.current = nextDrag;
    insertIndexRef.current = nextInsertIndex;
    setDrag(nextDrag);
    setInsertIndex(nextInsertIndex);
  };

  const moveLetterWithKeyboard = (event: React.KeyboardEvent<HTMLButtonElement>, index: number) => {
    if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
    event.preventDefault();
    const nextIndex = event.key === "ArrowLeft" ? index - 1 : index + 1;
    if (nextIndex < 0 || nextIndex >= letters.length) return;
    onMappingChange(reorderLetters(letters, index, nextIndex).join(""));
  };

  return (
    <div className="shuffle-editor">
      <div className="shuffle-grid">
        <span className="shuffle-track-label">{originalLabel}</span>
        <div className="shuffle-chip-row">
          {inputDictionary.split("").map((letter) => <span className="shuffle-chip shuffle-chip-static" key={`original-${letter}`}>{letter}</span>)}
        </div>
        <span className="shuffle-track-label">{mappingLabel}</span>
        <div
          className="shuffle-chip-row shuffle-chip-row-draggable"
          onPointerCancel={(event) => finishDrag(event.pointerId)}
          onPointerMove={moveDrag}
          onPointerUp={(event) => finishDrag(event.pointerId)}
          ref={rowRef}
        >
          {!drag ? letters.map((letter, index) => (
            <button aria-label={`Move ${letter}`} className="shuffle-chip shuffle-chip-draggable" key={`shuffle-${letter}`} onKeyDown={(event) => moveLetterWithKeyboard(event, index)} onPointerDown={(event) => startDrag(event, index, letter)} title="Drag, or use left and right arrow keys" type="button">{letter}</button>
          )) : (
            <>
              {remainingEntries.slice(0, insertIndex ?? 0).map((entry) => <button className="shuffle-chip shuffle-chip-draggable shuffle-chip-idle" data-shuffle-letter key={`shuffle-${entry.letter}`} tabIndex={-1} type="button">{entry.letter}</button>)}
              <span aria-hidden className="shuffle-chip shuffle-chip-gap" style={{ width: drag.width, minWidth: drag.width, height: drag.height }} />
              {remainingEntries.slice(insertIndex ?? 0).map((entry) => <button className="shuffle-chip shuffle-chip-draggable shuffle-chip-idle" data-shuffle-letter key={`shuffle-${entry.letter}`} tabIndex={-1} type="button">{entry.letter}</button>)}
            </>
          )}
        </div>
      </div>
      {drag && <span className="shuffle-chip shuffle-chip-draggable shuffle-chip-float" style={{ left: drag.x, top: drag.y, width: drag.width, height: drag.height }}>{drag.letter}</span>}
    </div>
  );
}
