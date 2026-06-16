from __future__ import annotations
import json
from pathlib import Path
import utils


def run(
    name: str = "T10",
    category: str = "Performance Scaling Analysis",
    section: str = "Strong Scaling",
    case: str = "Fixed ciphertext; np in [1,2,3,4,5,6,7,8]",
    plaintext: str = "But we can rebuild.",
    dictionary: Path = utils.ENGLISH_DICT_PATH,
    ranks_list = (1,2,3,4,5,6,7,8),
    reps: int = 30
) -> str:
    """
    ************************************************************************************************
    T10: Strong Scaling for MPI decrypt (decrypt-mpi)
    ************************************************************************************************
    """

    # run benchmark in MPI mode (even for np=1)
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
