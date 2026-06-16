from __future__ import annotations
from pathlib import Path
import utils

DEPTHS = list(range(1, 11));
RANKS = (2,3,4,5,6,7,8);

def _case_label(
    d: int
) -> str:
    return f"Fixed ciphertext; -d={d}; np in [2..8]";

def _name_for(
    d: int
) -> str:
    return f"T11_d{d:02d}";

def run(
    name: str = "T11 (bundle)",
    category: str = "Performance Scaling Analysis",
    section: str = "Strong Scaling (Manual Depths d=1..10)",
    plaintext: str = "Batman v Superman",
    dictionary: Path = utils.ENGLISH_DICT_PATH,
    reps: int = 15
) -> str:
    """
    ************************************************************************************************
    T11: Strong scaling with fixed -d (bundle)
        d in [1..10]
        np in [2..8] for each d
    ************************************************************************************************
    """
    paths = [];

    # run
    for d in DEPTHS:
        out_path = utils.run_benchmark(
            name=_name_for(d),
            category=category,
            section=f"Strong Scaling (Manual Depth d={d})",
            case=_case_label(d),
            plaintext=plaintext,
            dictionary=dictionary,
            ranks_list=list(RANKS),
            reps=reps,
            mode="mpi",
            valgrind=False,
            depth=d,
        );
        paths.append(str(out_path));

    return "\n".join(paths);

if __name__ == "__main__":
    print(run());
