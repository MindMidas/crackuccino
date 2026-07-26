from __future__ import annotations
from pathlib import Path
import utils


def run(
    name: str = "T7",
    category: str = "Correctness of Programs",
    section: str = "Result Verification & Dictionary Matching",
    case: str = "Valid match detection",
    plaintext: str = "hello world",
    dictionary: Path = utils.CUSTOM_DICT_PATH,
    ranks_list = (8,),
    reps: int = 2,
    expected_hits: int = 1,
) -> str:
    """
    ************************************************************************************************
    T7: verify valid_hits == 1 when the plaintext words are all present in the dict
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
        valgrind=False,
        expected_hits=expected_hits,
    );

    return str(out_path);


if __name__ == "__main__":
    print(run())
