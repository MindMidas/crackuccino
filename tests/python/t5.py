   
from __future__ import annotations
from pathlib import Path
import utils


def run(
    name: str = "T5",
    category: str = "Correctness of Programs",
    section: str = "Input Robustness & Normalization",
    case: str = "Punctuation",
    plaintext: str = "Vengeance! isn't, power.",
    dictionary: Path = utils.ENGLISH_DICT_PATH,
    ranks_list = (8,),
    reps: int = 2
) -> str:
    """
    ************************************************************************************************
    T5: confirm spaces and punctuation are preserved
    ************************************************************************************************
    """

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
        mode="mpi",
        valgrind=False
    );

    return str(out_path);


if __name__ == "__main__":
    print(run());