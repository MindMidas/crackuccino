from __future__ import annotations
import json, subprocess
import utils


def run(
    name: str = "T1",
    category: str = "Correctness of Programs",
    section: str = "Memory Safety & Stability",
    case: str = "Encrypt",
    plaintext: str = "You merely adopted the dark"
) -> str:
    """
    ************************************************************************************************
    T1: Valgrind mem/leak check for encrypt
    ************************************************************************************************
    """

    # val available
    if not utils.valgrind_available():
        raise RuntimeError("Valgrind not found on PATH");

    # resolve encrypt binary
    enc = utils._bin(utils.SRC_DIR / "encrypt");

    # run under val
    cp = subprocess.run(
        ["valgrind", "--leak-check=full", "--show-leak-kinds=all", enc, plaintext], 
        capture_output=True, text=True, check=True, cwd=str(utils.SRC_DIR)
    );

    # parse stdout to dict
    parsed = utils.parse_encrypt_stdout(cp.stdout);
    if not all(parsed.values()):
        raise RuntimeError(f"Failed to parse encrypt output:\n{cp.stdout}");

    # write inputs 
    utils.write_input_json(name, plaintext, parsed["input_dict"], parsed["encrypt_dict"], parsed["ciphertext"]);

    # parse val
    vg = utils.parse_valgrind_text(cp.stderr);

    # simple pass/fail on errors/leaks
    es = vg.get("error_summary") or 0;
    dl = vg.get("definitely_lost_bytes") or 0;
    il = vg.get("indirectly_lost_bytes") or 0;
    pl = vg.get("possibly_lost_bytes") or 0;
    passed = (es == 0) and (dl == 0) and (il == 0) and (pl == 0);

    # save result
    out = {
        "name": name,
        "category": category,
        "section": section,
        "case": case,
        "passed": passed,
        "plaintext": plaintext,
        "input": parsed,
        "valgrind": vg,
    };

    out_path = utils.RESULTS_DIR / f"{name}.json";
    out_path.write_text(json.dumps(out, indent=2));

    return str(out_path);

if __name__ == "__main__":
    print(run());
