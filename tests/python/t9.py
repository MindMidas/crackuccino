from __future__ import annotations
import json
from pathlib import Path
import utils

def run(
    name_a: str = "T9a",
    name_b: str = "T9b",
    category: str = "Performance Scaling Analysis",
    section: str = "Baseline",
    case_a: str = "Serial baseline (decrypt-serial, np=1)",
    case_b: str = "MPI baseline (decrypt-mpi, np=1)",
    
    plaintext: str = "I'm not afraid. I'm angry.",
    dictionary: Path = utils.ENGLISH_DICT_PATH,
    reps: int = 30,
) -> list[str]:
    """
    ************************************************************************************************
    T9 (Baseline):
        T9a: serial (decrypt-serial) with np=1
        T9b: mpi (decrypt-mpi) with np=1
    ************************************************************************************************
    """
    
    out_paths = [];

    # run serial baseline (np=1, mode="serial")
    out_a = utils.run_benchmark(
        name=name_a,
        category=category,
        section=section,
        case=case_a,
        plaintext=plaintext,
        dictionary=dictionary,
        ranks_list=[1],
        reps=reps,
        mode="serial",
        valgrind=False
    );

    out_paths.append(str(out_a));

    # run MPI baseline (np=1, mode="mpi")
    out_b = utils.run_benchmark(
        name=name_b,
        category=category,
        section=section,
        case=case_b,
        plaintext=plaintext,
        dictionary=dictionary,
        ranks_list=[1],
        reps=reps,
        mode="mpi",
        valgrind=False
    );

    out_paths.append(str(out_b));

    return out_paths;

if __name__ == "__main__":
    for p in run():
        print(p);
