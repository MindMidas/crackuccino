from __future__ import annotations
from pathlib import Path
import utils


def run(
    name: str = "T3",
    category: str = "Correctness of Programs",
    section: str = "Memory Safety & Stability",
    case: str = "MPI Decrypt",
    plaintext: str = "Why do we fall",
    dictionary: Path = utils.ENGLISH_DICT_PATH,
    ranks_list = (1, 2, 4, 8),
    reps: int = 3
) -> str:
    """
    ************************************************************************************************
    T3: Valgrind mem/leak check for MPI decrypt (decrypt-mpi) via utils.run_benchmark
    ************************************************************************************************
    """

    # val available
    if not utils.valgrind_available():
        raise RuntimeError("Valgrind not found on PATH");

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
