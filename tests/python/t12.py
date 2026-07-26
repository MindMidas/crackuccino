from __future__ import annotations
from pathlib import Path
import utils


QUOTES: tuple[str, ...] = (
    "I'm",                               # n=2
    "Bat",                               # n=3
    "Bane",                              # n=4
    "Joker",                             # n=5
    "I am Batman",                       # n=6
    "Justice",                           # n=7
    "Swear to me",                       # n=8
    "Why do we fall",                    # n=9
    "We betray one another",             # n=10
    "I'm not afraid. I'm angry.",        # n=11
    "But we can rebuild",                # n=12
)


def _plaintext_for_n(
    n: int
) -> str:

    if n < 2 or n > 12:
        raise ValueError("n must be in [2...12]");

    return QUOTES[n - 2];


def _np_for_n(
    n:int
) -> int:
    return min(max(n,2), 8);


def run(
    name: str = "T12",
    category: str = "Performance Scaling Analysis",
    section: str = "To Use Or Not To Use MPI",
    dictionary: Path = utils.ENGLISH_DICT_PATH,
    reps: int = 30
) -> str:
    """
    ************************************************************************************************
    T12: Compare serial (np=1) vs MPI with 'perfect subscription' as n grows
      n in [2...12]
      MPI ranks: [2,3,4,5,6,7,8,8,8,8,8] aligned to n
      Serial: np=1 for each n
    ************************************************************************************************
    """

    outputs = [];

    for n in range(2, 13):

        pt = _plaintext_for_n(n);

        # serial run (np=1)
        out_serial = utils.run_benchmark(
            name=f"T12_n{n:02d}_serial",
            category=category,
            section=section,
            case=f"Serial baseline; n={n}; np=1",
            plaintext=pt,
            dictionary=dictionary,
            ranks_list=[1],
            reps=reps,
            mode="serial",
            valgrind=False
        );

        outputs.append(str(out_serial));

        # MPI run with perfect subscription mapping
        np_val = _np_for_n(n)
        out_mpi = utils.run_benchmark(
            name=f"T12_n{n:02d}_mpi",
            category=category,
            section=section,
            case=f"MPI; n={n}; np={np_val} (perfect subscription mapping)",
            plaintext=pt,
            dictionary=dictionary,
            ranks_list=[np_val],
            reps=reps,
            mode="mpi",
            valgrind=False
        );

        outputs.append(str(out_mpi));

    return "\n".join(outputs);

if __name__ == "__main__":
    print(run());
