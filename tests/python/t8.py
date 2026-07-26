from __future__ import annotations
from pathlib import Path
import utils


def run(
    name: str = "T8",
    category: str = "Correctness of Programs",
    section: str = "Result Verification & Dictionary Matching",
    case: str = "No match detection",
    plaintext: str = "hello wurld",
    dictionary: Path = utils.CUSTOM_DICT_PATH,
    ranks_list = (8,),
    reps: int = 2,
    expected_hits: int = 1
) -> str:
    """
    ************************************************************************************************
    T8: verify valid_hits == 0 when at least one plaintext word is not in the dictionary.
    ************************************************************************************************
    """

    # run benchmark (MPI mode)
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
        valgrind=False,
        expected_hits=0
    )

    return str(out_path)


if __name__ == "__main__":
    print(run())
