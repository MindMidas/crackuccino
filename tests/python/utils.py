from __future__ import annotations

import os
import re
import json
import time
import pathlib
import statistics
import subprocess
import shutil
import glob  
from typing import List, Dict, Any, Optional, Tuple


# paths & locations

# dir containing this file (tests/python/)
HERE: pathlib.Path = pathlib.Path(__file__).resolve().parent;

# tests/ (one level above)
ROOT: pathlib.Path = HERE.parent;

# input and results folders (created if missing)
INPUT_DIR: pathlib.Path = ROOT / "inputs";
RESULTS_DIR: pathlib.Path = ROOT / "results";
INPUT_DIR.mkdir(parents=True, exist_ok=True);
RESULTS_DIR.mkdir(parents=True, exist_ok=True);

# project root (one level above tests/)
PROJ_ROOT: pathlib.Path = ROOT.parent;

# src/ with compiled binaries (encrypt, decrypt-serial, decrypt-mpi)
SRC_DIR: pathlib.Path = PROJ_ROOT;

# bundled dictionaries
DICTIONARIES_DIR: pathlib.Path = PROJ_ROOT / "src" / "data" / "dictionaries";
ENGLISH_DICT_PATH: pathlib.Path = DICTIONARIES_DIR / "american_english_dictionary";
CUSTOM_DICT_PATH: pathlib.Path = DICTIONARIES_DIR / "small_dictionary";


def _bin(
    path: pathlib.Path
) -> str:
    """
    ************************************************************************************************
    resolve a binary path to string (and make sure it exists)

    Params:
        path : pathlib.Path
            filesystem path to executable
    Returns:
        str
            absolute str path for subprocess calls
    Raises:
        FileNotFoundError
            if path does not exist
    ************************************************************************************************
    """

    p = path.resolve();

    if not p.exists():
        raise FileNotFoundError(f"binary not found: {p}");

    return str(p);



def _save_text(
    path: pathlib.Path,
    text: str
) -> pathlib.Path:

    """
    ************************************************************************************************
    write text to a file (creates parent directories)

    Params:
        path : pathlib.Path
            destination file path
        text : str
            str to write
    Returns:
        pathlib.Path
            the path written
    ************************************************************************************************
    """

    # ensure folder exists
    path.parent.mkdir(parents=True, exist_ok=True);

    # create if missing, otherwise overwrite
    path.write_text(text);

    return path;



def _summ_stats(
    xs: List[float]
) -> Dict[str, float]:
    """
    ************************************************************************************************
    compute stats for a list of floats

    Params:
        xs : list[float]
    Returns:
        dict
            { "min": float, "mean": float, "max": float, "stdev": float }
    ************************************************************************************************
    """

    # no list
    if not xs:
        return {"min": 0.0, "mean": 0.0, "max": 0.0, "stdev": 0.0};
    
    # calc min, max, mean, and stdev
    mean = sum(xs) / len(xs);
    stdev = statistics.pstdev(xs) if len(xs) > 1 else 0.0;

    return {"min": min(xs), "mean": mean, "max": max(xs), "stdev": stdev};



def write_input_json(
    name: str,
    plaintext: str,
    input_dict: str,
    encrypt_dict: str,
    ciphertext: str,
) -> pathlib.Path:
    """
    ************************************************************************************************
    write input JSON for Crackuccino under tests/inputs/<name>.json

    Schema:
        {
            "name": "<test_name>",
            "plaintext": "<str>",
            "input_dict": "<letters>",
            "encrypt_dict": "<letters>",
            "ciphertext": "<str>"
        }
    Params:
        name : str
            label, ex: "T1"
        plaintext : str
            original message to encrypt
        input_dict : str
            n unique letters from plaintext
        encrypt_dict : str
            shuffled mapping used by encrypt
        ciphertext : str
            produced ciphertext for plaintext
    Returns:
        pathlib.Path
            path written
    ************************************************************************************************
    """

    payload = {
        "name": name,
        "plaintext": plaintext,
        "input_dict": input_dict,
        "encrypt_dict": encrypt_dict,
        "ciphertext": ciphertext,
    };

    out = INPUT_DIR / f"{name}.json";
    
    return _save_text(out, json.dumps(payload, indent=2));



def parse_encrypt_stdout(
    stdout_text: str
) -> Dict[str, str]:
    """
    ************************************************************************************************
    parse encrypt stdout to extract input_dict, encrypt_dict, and ciphertext

    Returns:
        dict with keys:
            "input_dict": str,
            "encrypt_dict": str,
            "ciphertext": str
    ************************************************************************************************
    """

    fields = {"input_dict": "", "encrypt_dict": "", "ciphertext": ""};

    for line in stdout_text.splitlines():
        line = line.strip();

        if line.startswith("input_dict:"):
            fields["input_dict"] = line.split(":", 1)[1].strip();

        elif line.startswith("encrypt_dict:"):
            fields["encrypt_dict"] = line.split(":", 1)[1].strip();

        elif line.startswith("ciphertext:"):
            fields["ciphertext"] = line.split(":", 1)[1].strip();

    return fields



def run_encrypt(
    name: str,
    plaintext: str
) -> pathlib.Path:
    """
    ************************************************************************************************
    run encrypt on plaintext, captures stdout, and write input JSON using write_input_json()

    Params:
        name : str
            label, ex: "T1"
        plaintext : str
            message to encrypt
    Returns:
        pathlib.Path
            path written
    ************************************************************************************************
    """
    encrypt_bin = _bin(SRC_DIR / "encrypt")

    # run encrypt and capture output
    cp = subprocess.run(
        [encrypt_bin, plaintext],
        capture_output=True, text=True, check=True,
        cwd=str(SRC_DIR)
    );

    # parse fields from stdout
    parsed = parse_encrypt_stdout(cp.stdout);

    # check
    if not all(parsed.values()):
        raise RuntimeError(f"Failed to parse output from encrypt:\n{cp.stdout}");

    # write input JSON for report
    return write_input_json(name, plaintext, parsed["input_dict"], parsed["encrypt_dict"], parsed["ciphertext"],);



def _extract_summary(
    stdout_text: str
) -> Optional[Dict[str, Any]]:
    """
    ************************************************************************************************
    parse the summary block printed by serial & mpi decrypt programs

    Expected format:
        [rank X] [permutation: Y] found: Z
        ...
        ************************************************************************************************
        [mpi or serial] permutation summary:
        ranks: <int>
        n_letters: <int>
        depth: <int>
        expected: <uint>
        visited: <uint>
        valid_hits: <uint>
        runtime_sec: <float>

    Params:
        stdout_text : str
            stdout captured from program
    Returns:
        dict with keys:
            "ranks": int,
            "n_letters": int,
            "depth": int,
            "expected_perms": int,
            "visited_perms": int,
            "valid_hits": int,
            "runtime_sec": float,
            "found": [str, ...]   # decoded plaintexts found
        or None if no block found
    ************************************************************************************************
    """

    # setup
    lines = [line.strip() for line in stdout_text.splitlines()];
    in_block = False;
    data: Dict[str, Any] = {};
    needed = {"ranks", "n_letters", "depth", "expected", "visited", "valid_hits", "runtime_sec"};
    found: list[str] = [];

    # first pass: collect all found lines
    for line in lines:
        if "[permutation:" in line and "found:" in line:
            found.append(line.strip());

    # second pass: parse summary block
    for line in lines:

        # header line marks start
        if line.endswith("permutation summary:"):
            in_block = True;
            continue;

        if not in_block:
            continue;

        # footer line marks end
        if line.startswith("************************************************************************************************"):
            break;

        # parse key:value
        if ":" in line:
            k, v = line.split(":", 1);
            k = k.strip();
            v = v.strip();
            if k in needed:
                data[k] = v;

    # require all summary fields
    if not needed.issubset(data.keys()):
        return None;

    try:
        return {
            "ranks": int(data["ranks"]),
            "n_letters": int(data["n_letters"]),
            "depth": int(data["depth"]),
            "expected_perms": int(data["expected"]),
            "visited_perms": int(data["visited"]),
            "valid_hits": int(data["valid_hits"]),
            "runtime_sec": float(data["runtime_sec"]),
            "found": found
        };

    except Exception:
        return None;



def valgrind_available(

) -> bool:
    """
    ************************************************************************************************
    Returns:
        True if 'valgrind' is on PATH, else False
    ************************************************************************************************
    """
    return shutil.which("valgrind") is not None



def parse_valgrind_text(
    text: str
) -> Dict[str, Any]:
    """
    ************************************************************************************************
    parse key leak/error lines from valgrind log

    Returns:
        dict with keys:
            "error_summary": int or None
            "definitely_lost_bytes": int or None
            "indirectly_lost_bytes": int or None
            "possibly_lost_bytes": int or None
            "still_reachable_bytes": int or None
            "raw": str  (full text)
    ************************************************************************************************
    """

    out = {
        "error_summary": None,
        "definitely_lost_bytes": None,
        "indirectly_lost_bytes": None,
        "possibly_lost_bytes": None,
        "still_reachable_bytes": None,
        "raw": text,
    };
    
    # helper
    def _bytes(line: str) -> Optional[int]:
        # lines look like: definitely lost: 0 bytes in 0 blocks, etc
        m = re.search(r":\s*([0-9,]+)\s+bytes", line);
        return int(m.group(1).replace(",", "")) if m else None;

    # iterate over lines
    for line in text.splitlines():

        s = line.strip().lower();

        if s.startswith("error summary:"):
            m = re.search(r"error summary:\s*([0-9,]+)\s+errors", s);
            if m: out["error_summary"] = int(m.group(1).replace(",", ""));

        elif s.startswith("definitely lost:"):
            out["definitely_lost_bytes"] = _bytes(line) or out["definitely_lost_bytes"];

        elif s.startswith("indirectly lost:"):
            out["indirectly_lost_bytes"] = _bytes(line) or out["indirectly_lost_bytes"];

        elif s.startswith("possibly lost:"):
            out["possibly_lost_bytes"] = _bytes(line) or out["possibly_lost_bytes"];
            
        elif s.startswith("still reachable:"):
            out["still_reachable_bytes"] = _bytes(line) or out["still_reachable_bytes"];


    return out;



def _get_physical_cores(

) -> int:
    """
    ************************************************************************************************
    try and get physical core count, fallback os.cpu_count()

    Returns:
        int
    ************************************************************************************************
    """
    try:
        out = subprocess.check_output("lscpu", text=True);
        sockets = cores = 1;

        for line in out.splitlines():

            if "Socket(s):" in line:
                sockets = int(re.findall(r"\d+", line)[0]);

            elif "Core(s) per socket:" in line:
                cores = int(re.findall(r"\d+", line)[0]);

        return sockets * cores;

    except Exception:

        # fallback
        return os.cpu_count() or 1;



def _run_serial(
    dictionary: pathlib.Path,
    valgrind: bool = False,
) -> Tuple[str, Dict[str, Any], Optional[Dict[str, Any]]]:
    """
    ************************************************************************************************
    run a single serial decryption and parse summary

    Params:
        dictionary: pathlib.Path
            path to input dictionary 
        valgrind: bool
            True or False if you want to run Valgrind
    Returns:
        tuple[str, dict, (dict or None)]:
            stdout_text,
            summary_dict:
                ranks, n_letters, expected_perms, visited_perms, valid_hits, runtime_sec, found
            vg_dict:
                error_summary, definitely_lost_bytes, indirectly_lost_bytes, possibly_lost_bytes, 
                still_reachable_bytes, raw
    ************************************************************************************************
    """

    # binary
    serial_bin = _bin(SRC_DIR / "decrypt-serial");

    # base command
    base = [serial_bin, "ciphertext.txt", str(dictionary), "-s"];

    # add val flag
    if valgrind:
        if not valgrind_available():
            raise RuntimeError("Valgrind not found on PATH");
        cmd = ["valgrind", "--leak-check=full", "--show-leak-kinds=all"] + base;
    else:
        cmd = base;

    # run
    cp = subprocess.run(cmd, capture_output=True, text=True, check=True, cwd=str(SRC_DIR));

    # get summary
    summary = _extract_summary(cp.stdout) or {};
    vg = parse_valgrind_text(cp.stderr) if valgrind else None;

    return cp.stdout, summary, vg;


def _run_mpi(
    dictionary: pathlib.Path,
    nranks: int,
    valgrind: bool = False,
    depth: Optional[int] = None
) -> Tuple[str, Dict[str, Any], Optional[Dict[str, Any]]]:
    """
    ************************************************************************************************
    run a single MPI decryption and parse summary

    Params:
        dictionary: pathlib.Path
            path to input dictionary 
        nranks : int
            num MPI ranks to run
        valgrind: bool
            True or False if you want to run Valgrind
        depth: Optional[int]
            prefix len for partial permutation, default None
    Returns:
        tuple[str, dict, (dict or None)]:
            stdout_text,
            summary_dict:
                ranks, n_letters, expected_perms, visited_perms, valid_hits, runtime_sec, found
            vg_dict:
                error_summary, definitely_lost_bytes, indirectly_lost_bytes, possibly_lost_bytes, 
                still_reachable_bytes, raw
    ************************************************************************************************
    """

    # binary
    mpi_bin = _bin(SRC_DIR / "decrypt-mpi");

    # base command
    base = [mpi_bin, "ciphertext.txt", str(dictionary), "-s"];

    # add depth flag
    if depth is not None:
        base += ["-d", str(depth)];

    # add val
    if valgrind:
        if not valgrind_available():
            raise RuntimeError("Valgrind not found on PATH");
        
        # valgrind wraps per-rank exec
        cmd = [
            "mpirun", "-np", str(nranks),
            "valgrind", "--leak-check=full", "--show-leak-kinds=all",
            "--log-file=valgrind.%p.log",
            *base
        ];

    else:
        cmd = ["mpirun", "-np", str(nranks), *base];

    # run and get summary
    cp = subprocess.run(cmd, capture_output=True, text=True, check=True, cwd=str(SRC_DIR));
    summary = _extract_summary(cp.stdout) or {};

    vg = None;

    # add val logs
    if valgrind:

        logs = sorted(glob.glob(str((SRC_DIR / "valgrind.*.log").resolve())));
        combined = "";

        for p in logs:
            try:
                combined += pathlib.Path(p).read_text() + "\n";
            except Exception:
                pass;

        vg = parse_valgrind_text(combined);

        for p in logs:
            os.remove(p); # remove val logs

    return cp.stdout, summary, vg;



def _consistent_metrics(
    a: Dict[str, Any],
    b: Dict[str, Any]
) -> bool:
    """
    ************************************************************************************************
    verify n_letters, expected_perms, visited_perms, valid_hits, and found (order-insensitive)
    are consistent across reps

    Params:
        a : dict
            1st summary dict
        b : dict
            2nd summary dict
    Returns:
        bool
            True if consistent, else False
    ************************************************************************************************
    """

    # check consistency of numbers first
    for k in ("n_letters", "expected_perms", "visited_perms", "valid_hits"):
        if a.get(k) != b.get(k):
            return False;

    # check found list consistency (order-insensitive)
    found_a = set(a.get("found", []));
    found_b = set(b.get("found", []));

    if found_a != found_b:
        return False;

    return True;



def run_benchmark(
    name: str,
    category: str,
    section: str,
    case: str,
    plaintext: str,
    dictionary: pathlib.Path,
    ranks_list: List[int],
    reps: int = 30,
    mode: str = "mpi",
    valgrind: bool = False,
    expected_hits: Optional[int] = None,
    depth: Optional[int] = None,
) -> pathlib.Path:
    """
    ************************************************************************************************
    runner for Crackuccino:
        encrypt and write to tests/inputs
        for each ranks config, run reps repetitions
        collect runtime_sec from program summary for each rep
        ensure output metrics (n_letters, expected_perms, visited_perms, valid_hits, found) are consistent across reps
        write results json to tests/results

    Output JSON schema example:
        {
            "name": "T1",
            "category": "Correctness of Programs",
            "section": "Memory Safety & Stability",
            "case": "encrypt memcheck",
            "passed": true,
            "plaintext": "plaintext",
            "reps": 30,
            "detected_cores": 11,
            "total_ranks": 5,
            "ciphertext": "hello",
            "n_letters": 4,
            "expected_perms": 24,
            "visited_perms": 24,
            "valid_hits": 1,
            "found": [str, ...]   # decoded plaintexts found
            "configs": [
                { "ranks": 1,  "times_sec": [...], "summary": {...} },
                { "ranks": 2,  "times_sec": [...], "summary": {...} },
                ...
            ]
        }

    Params:
        name : str
            label (ex:, "T1")
        plaintext : str
            message to encrypt
        dictionary: pathlib.Path
            path to input dictionary 
        ranks_list : list[int]
            ex: [1, 2, 4, 8, 11]
        reps : int
            number of repetitions per config
        mode : str
            "serial" or "mpi"
                "serial": when nranks == 1, use serial binary; otherwise use MPI.
                "mpi": always use MPI (even for nranks == 1).
        valgrind: bool
            True or False if you want to run Valgrind
        expected_hits : Optional[int]
            assert that program's reported_valid_hits == expected_hits
        depth: Optional[int]
            prefix len for partial permutation, default None
    Returns:
        pathlib.Path
            path to the final results JSON
    ************************************************************************************************
    """

    # mode check
    mode = (mode or "mpi").lower();
    if mode not in ("serial", "mpi"):
        raise ValueError('mode must be either "serial" or "mpi"');

    # encrypt plaintext, write to test/inputs, and read ciphertext
    try:
        input_json_path = run_encrypt(name, plaintext);
        payload = json.loads(input_json_path.read_text());
        ciphertext = str(payload.get("ciphertext", "")).strip();
    except Exception as e:
        print(f"[run_benchmark] failed to load ciphertext: {e}");
        ciphertext = "";


    # init results dict
    result: Dict[str, Any] = {
        "name": name,
        "category": category,
        "section": section,
        "case": case,
        "passed": True,
        "plaintext": plaintext,
        "valgrind_enabled": bool(valgrind),
        "reps": reps,
        "detected_cores": _get_physical_cores(),
        "total_ranks": len(ranks_list),
        "ciphertext": ciphertext,
        "n_letters": None,
        "depth": None,
        "expected_perms": None,
        "visited_perms": None,
        "valid_hits": None,
        "found": None,
        "configs": [],
    };

    # baseline for cmp
    baseline: Optional[Dict[str, Any]] = None;

    # loop over rank configurations
    for nranks in ranks_list:

        times: List[float] = [];
        last_vg: Optional[Dict[str, Any]] = None;
        cfg_baseline: Optional[Dict[str, Any]] = None;

        # helper: choose runner per mode
        def _run_once(

        ) -> Tuple[str, Dict[str, Any], Optional[Dict[str, Any]]]:

            if nranks == 1 and mode == "serial":
                return _run_serial(dictionary=dictionary, valgrind=valgrind);
            else:
                return _run_mpi(dictionary=dictionary, depth=depth, nranks=nranks, valgrind=valgrind);


        # run reps times
        for _ in range(reps):

            # run one rep
            stdout_text, summary, vg = _run_once();
            
            # set config-local baseline
            if cfg_baseline is None and summary:
                cfg_baseline = summary;
            else:
                if not _consistent_metrics(cfg_baseline, summary):
                    result["passed"] = False;

            # capture rep valgrind summary
            if vg is not None:
                last_vg = vg;

            # must have summary
            if not summary:
                result["passed"] = False;

            # record runtime for this rep
            rt = summary.get("runtime_sec", None);
            if isinstance(rt, (int, float)):
                times.append(float(rt));
            else:
                # if runtime missing, mark as failed but still proceed
                result["passed"] = False;

            # check consistency baseline
            if baseline is None and summary:
                baseline = summary;
            else:
                if not _consistent_metrics(baseline, summary):
                    result["passed"] = False;


        # set global fields using baseline
        if result["n_letters"] is None and baseline:
            result["n_letters"] = baseline.get("n_letters", None);
            result["depth"] = baseline.get("depth", None);
            result["expected_perms"] = baseline.get("expected_perms", None);
            result["visited_perms"] = baseline.get("visited_perms", None);
            result["valid_hits"] = baseline.get("valid_hits", None);
            result["found"] = baseline.get("found", None);

        # append per-config block
        cfg_block = {
            "ranks": nranks,
            "depth": cfg_baseline["depth"],
            "times_sec": times,
            "summary": _summ_stats(times)
        };

        # append and check valgrind
        if valgrind:
                cfg_block["valgrind"] = last_vg or {};

                # if any errors -> failed
                if last_vg:
                    es = last_vg.get("error_summary");
                    dl = last_vg.get("definitely_lost_bytes");
                    il = last_vg.get("indirectly_lost_bytes");
                    pl = last_vg.get("possibly_lost_bytes");
                    if (isinstance(es, int) and es > 0) or any(
                        isinstance(x, int) and x > 0 for x in (dl, il, pl)
                    ):
                        result["passed"] = False;

        result["configs"].append(cfg_block);

    # assert expected hits
    if expected_hits is not None and result["valid_hits"] is not None:
        if int(result["valid_hits"]) != int(expected_hits):
            result["passed"] = False;
    
    # write json result
    out_path = RESULTS_DIR / f"{name}.json";
    _save_text(out_path, json.dumps(result, indent=2));
    
    return out_path;
