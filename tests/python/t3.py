from __future__ import annotations
import json
from pathlib import Path
import utils


def run(
    name: str = "T3",
    category: str = "Correctness of Programs",
    section: str = "Memory Safety & Stability",
    case: str = "Crackuccino MPI decrypt",
    plaintext: str = "Why do we fall",
    dictionary: Path = utils.ENGLISH_DICT_PATH,
    ranks_list = (1, 2, 4, 8),
    reps: int = 3
) -> str:
    """
    ************************************************************************************************
    T3: Valgrind mem/leak check for MPI decrypt via utils.run_benchmark
    ************************************************************************************************
    """

    # val available
    if not utils.valgrind_available():
        out_path = utils.RESULTS_DIR / f"{name}.json";
        out_path.write_text(json.dumps({
            "name": name,
            "category": category,
            "section": section,
            "case": case,
            "passed": True,
            "skipped": True,
            "reason": "Valgrind not found on PATH",
        }, indent=2));
        return str(out_path);

    # run benchmark
    out_path = utils.run_benchmark(
        name=name,
        category=category,
        section=section,
        case=case,
        plaintext=plaintext,
        dictionary=dictionary,
        ranks_list=list(ranks_list),
        reps=reps,
        mode="mpi", # use MPI (even for ranks=1)
        valgrind=True
    );

    return str(out_path);

if __name__ == "__main__":
    print(run())
