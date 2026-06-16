from __future__ import annotations
from pathlib import Path
import utils


def run(
    name: str = "T2",
    category: str = "Correctness of Programs",
    section: str = "Memory Safety & Stability",
    case: str = "Serial Decrypt",
    plaintext: str = "Swear to me",
    dictionary: Path = utils.ENGLISH_DICT_PATH,
    reps: int = 3
) -> str:
    """
    ************************************************************************************************
    T2: Valgrind mem/leak check for serial decrypt (decrypt-serial) via utils.run_benchmark
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
        ranks_list=[1], # serial config only
        reps=reps,
        mode="serial", # use serial binary
        valgrind=True # enable val
    );

    return str(out_path);

if __name__ == "__main__":
    print(run());
