#!/usr/bin/env python3

# Local HTTP server for Crackuccino decryption and dictionary APIs.

import argparse;
import hashlib;
import hmac;
import json;
import math;
import mimetypes;
import os;
import re;
import secrets;
import signal;
import shutil;
import subprocess;
import sys;
import tempfile;
import threading;
import time;
import uuid;
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer;
from http.cookies import CookieError, SimpleCookie;
from pathlib import Path;
from urllib.parse import unquote, urlparse;


PROJECT_ROOT = Path(__file__).resolve().parents[2];
FRONTEND_ROOT = PROJECT_ROOT / "src" / "frontend" / "dist";
DICTIONARIES_ROOT = PROJECT_ROOT / "src" / "data" / "dictionaries";
MAX_BODY_BYTES = 20_000;
MAX_DICTIONARY_BODY_BYTES = 250_000;
MAX_DICTIONARY_FILE_BYTES = 180_000;
MAX_TEXT_LENGTH = 240;
STALE_CLIENT_SECONDS = 6;
PROCESS_TERMINATE_GRACE_SECONDS = 2;
MAX_BRUTE_FORCE_PERMUTATIONS = 1_000_000_000;
MAX_PERMUTATION_TRACE_PER_RANK = 2_048;
ALLOWED_DICTIONARIES = {"brew_dictionary", "american_english_dictionary", "small_dictionary"};
BUILTIN_DICTIONARIES = {
    "brew_dictionary": "Brew",
    "american_english_dictionary": "American English",
    "small_dictionary": "Small",
};
CUSTOM_DICTIONARY_PREFIX = "custom:";
MAX_CUSTOM_WORDS = 5_000;
MAX_WORD_LENGTH = 80;
DICTIONARY_WORD_PATTERN = re.compile(r"^[A-Za-z]+(?:'[A-Za-z]+)*$");
RESULT_PATTERN = re.compile(r"^\[rank (\d+)\] \[permutation: ([^\]]*)\] found: (.*)$");
MPI_NETWORK_ERROR_MARKERS = (
    "getifaddrs() failed",
    "no available interfaces were found",
    "no network interfaces were found",
    "no sockets were able to be opened",
    "bind() failed for port",
);
RUNS = {};
RUNS_LOCK = threading.Lock();
MAX_RUN_HISTORY = 30;
SESSION_COOKIE_NAME = "crackuccino_session";
SESSION_SECRET = (os.environ.get("CRACKUCCINO_SESSION_SECRET") or "").encode("utf-8") or secrets.token_bytes(32);
MAX_ACTIVE_RUNS_PER_SESSION = 1;
MAX_ACTIVE_RUNS_GLOBAL = 4;
RATE_LIMITS = {
    "runs": (4, 60.0),
    "encrypt": (12, 60.0),
    "direct_decrypt": (12, 60.0),
    "dictionary_upload": (6, 60.0),
};
RATE_LIMIT_BUCKETS = {};
RATE_LIMIT_LOCK = threading.Lock();


def security_log(event, **fields):
    """
    Emit one structured security-relevant event without logging request bodies.
    """

    payload = {
        "event": event,
        "app": "crackuccino",
        "ts": round(time.time(), 3),
    };
    payload.update({key: value for key, value in fields.items() if value is not None});
    print(json.dumps(payload, sort_keys=True), file=sys.stderr);


class RateLimitError(RuntimeError):
    """
    Raised when an anonymous session or client IP exceeds a quota.
    """


def is_production():
    """
    Return whether production safety checks should be enforced.
    """

    return os.environ.get("CRACKUCCINO_ENV", "development").strip().lower() == "production";


def validate_production_security():
    """
    Fail fast when production is enabled without a stable session secret.
    """

    if not is_production():
        return;
    secret = os.environ.get("CRACKUCCINO_SESSION_SECRET", "").strip();
    if len(secret) < 32:
        raise RuntimeError("CRACKUCCINO_SESSION_SECRET must be at least 32 characters when CRACKUCCINO_ENV=production.");


def validate_bind_safety(host):
    """
    Reject accidental public binds without production checks.
    """

    if host in {"0.0.0.0", "::"} and not is_production():
        security_log("public_bind_without_production", host=host);
        raise RuntimeError("Refusing public bind without CRACKUCCINO_ENV=production.");


def check_rate_limit(scope, session_id, client_ip):
    """
    Apply fixed-window quotas by anonymous session and client IP.
    """

    limit, window = RATE_LIMITS[scope];
    now = time.monotonic();
    with RATE_LIMIT_LOCK:
        for key in ((scope, "session", session_id), (scope, "ip", client_ip)):
            bucket = RATE_LIMIT_BUCKETS.setdefault(key, []);
            bucket[:] = [timestamp for timestamp in bucket if now - timestamp < window];
            if len(bucket) >= limit:
                security_log("rate_limit_hit", scope=scope, limit=limit, window=window, clientIp=client_ip);
                raise RateLimitError("Too many requests. Please wait and try again.");
        for key in ((scope, "session", session_id), (scope, "ip", client_ip)):
            RATE_LIMIT_BUCKETS[key].append(now);


def require_string(payload, key, max_length=MAX_TEXT_LENGTH):
    """
    Read one required, bounded string from an API payload.
    """

    value = payload.get(key);
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{key} must be a non-empty string.");
    if len(value) > max_length:
        raise ValueError(f"{key} must be {max_length} characters or fewer.");
    return value.strip();


def dictionary_word_count(path):
    """
    Count non-empty dictionary lines without loading large files into memory.
    """

    try:
        with path.open("r", encoding="utf-8") as file:
            return sum(1 for line in file if line.strip());
    except OSError:
        return 0;


def dictionary_entry(identifier, label, source, path):
    """
    Build one dictionary metadata object for the frontend.
    """

    return {
        "id": identifier,
        "label": label,
        "source": source,
        "wordCount": dictionary_word_count(path),
    };


def list_dictionaries():
    """
    Return built-in dictionaries available from the server.
    """

    return [
        dictionary_entry(identifier, label, "builtin", DICTIONARIES_ROOT / identifier)
        for identifier, label in BUILTIN_DICTIONARIES.items()
    ];


def dictionary_path(identifier):
    """
    Resolve a built-in dictionary id to a known local file path.
    """

    if identifier in ALLOWED_DICTIONARIES:
        return DICTIONARIES_ROOT / identifier;
    return None;


def read_dictionary_detail(identifier, limit=800):
    """
    Return metadata and a bounded word preview for one dictionary.
    """

    path = dictionary_path(identifier);
    if path is None:
        raise ValueError("Dictionary not found.");

    words = [];
    total = 0;
    with path.open("r", encoding="utf-8") as file:
        for line in file:
            word = line.strip();
            if not word:
                continue;
            total += 1;
            if len(words) < limit:
                words.append(word);

    return {
        "id": identifier,
        "label": BUILTIN_DICTIONARIES[identifier],
        "source": "builtin",
        "wordCount": total,
        "words": words,
        "truncated": total > len(words),
    };


def safe_dictionary_stem(filename):
    """
    Convert an uploaded filename into a stable custom dictionary stem.
    """

    name = Path(filename).name;
    if not name.lower().endswith(".txt"):
        raise ValueError("Dictionary uploads must be .txt files.");
    stem = Path(name).stem.lower();
    stem = re.sub(r"[^a-z0-9]+", "-", stem).strip("-");
    if not stem:
        raise ValueError("Dictionary filename must include letters or numbers.");
    return stem[:48];


def normalize_dictionary_content(content):
    """
    Validate the one-word-per-line format expected by the native loader.
    """

    if not isinstance(content, str) or not content.strip():
        raise ValueError("Dictionary content must be a non-empty UTF-8 text file.");
    if len(content.encode("utf-8")) > MAX_DICTIONARY_FILE_BYTES:
        raise ValueError(f"Dictionary file must be {MAX_DICTIONARY_FILE_BYTES // 1000} KB or smaller.");

    words = [];
    seen = set();
    for line_number, raw_line in enumerate(content.splitlines(), start=1):
        word = raw_line.strip();
        if not word:
            continue;
        if len(word) > MAX_WORD_LENGTH:
            raise ValueError(f"Line {line_number} is too long. Words must be {MAX_WORD_LENGTH} characters or fewer.");
        if not DICTIONARY_WORD_PATTERN.fullmatch(word):
            raise ValueError(f"Line {line_number} must contain one a-z word; apostrophes are allowed inside words.");
        normalized = word.lower();
        if normalized not in seen:
            seen.add(normalized);
            words.append(normalized);
        if len(words) > MAX_CUSTOM_WORDS:
            raise ValueError(f"Custom dictionaries can contain at most {MAX_CUSTOM_WORDS} unique words.");

    if not words:
        raise ValueError("Dictionary must contain at least one word.");
    return words;


def upload_dictionary(payload):
    """
    Validate a custom dictionary and return normalized content for browser session storage.
    """

    filename = require_string(payload, "filename", 120);
    content = payload.get("content");
    stem = safe_dictionary_stem(filename);
    words = normalize_dictionary_content(content);
    normalized_content = "\n".join(words) + "\n";
    return {
        "dictionary": {
            "id": f"{CUSTOM_DICTIONARY_PREFIX}{stem}-{uuid.uuid4().hex[:8]}",
            "label": stem.replace("-", " ").title(),
            "source": "custom",
            "wordCount": len(words),
            "words": words[:800],
            "truncated": len(words) > 800,
            "content": normalized_content,
        }
    };


def validate_custom_dictionary_payload(payload, dictionary):
    """
    Re-validate a session-only custom dictionary before one MPI run uses it.
    """

    if not re.fullmatch(r"custom:[a-z0-9][a-z0-9-]{0,62}", dictionary):
        raise ValueError("custom dictionary id is invalid.");
    custom = payload.get("customDictionary");
    if not isinstance(custom, dict):
        raise ValueError("customDictionary is required for uploaded dictionaries.");
    if custom.get("id") != dictionary:
        raise ValueError("customDictionary id must match dictionary.");
    label = custom.get("label", "Custom Dictionary");
    if not isinstance(label, str) or len(label.strip()) > 80:
        raise ValueError("customDictionary label must be 80 characters or fewer.");
    content = custom.get("content");
    words = normalize_dictionary_content(content);
    return {
        "id": dictionary,
        "label": label.strip() or "Custom Dictionary",
        "words": words,
    };


def parse_encrypt_output(plaintext, output):
    """
    Convert encrypt stdout into the browser response schema.
    """

    fields = {};
    for line in output.splitlines():
        if ":" in line:
            key, value = line.split(":", 1);
            fields[key.strip()] = value.strip();

    required = ("input_dict", "encrypt_dict", "ciphertext");
    if not all(fields.get(key) is not None for key in required):
        raise RuntimeError("Could not parse encrypt output.");

    return {
        "plaintext": plaintext,
        "inputDictionary": fields["input_dict"],
        "encryptionDictionary": fields["encrypt_dict"],
        "ciphertext": fields["ciphertext"],
    };


def build_input_dictionary(text):
    """
    Match the C encryptor's unique lowercase letter dictionary.
    """

    seen = set();
    letters = [];
    for char in text.lower():
        if "a" <= char <= "z" and char not in seen:
            seen.add(char);
            letters.append(char);
    return "".join(letters);


def validate_encrypt_mapping(payload, plaintext):
    """
    Validate an optional user-supplied shuffled mapping for encryption.
    """

    value = payload.get("encryptionDictionary");
    if value is None:
        return None;
    if not isinstance(value, str) or not value.strip():
        raise ValueError("encryptionDictionary must be a non-empty string.");

    input_dictionary = build_input_dictionary(plaintext);
    mapping = value.strip().lower();

    if not input_dictionary:
        raise ValueError("Plaintext must contain letters before using a custom mapping.");
    if not re.fullmatch(r"[a-z]+", mapping):
        raise ValueError("Custom mapping must use only letters a-z.");
    if len(mapping) != len(input_dictionary):
        raise ValueError(f"Custom mapping must be {len(input_dictionary)} letters long.");
    if sorted(mapping) != sorted(input_dictionary):
        raise ValueError(f"Custom mapping must use exactly these letters: {input_dictionary}.");
    if len(input_dictionary) > 1 and mapping == input_dictionary:
        raise ValueError("Custom mapping must change the letter order.");

    return mapping;


def run_encrypt(payload):
    """
    Validate plaintext and run the existing C encryption binary.
    """

    plaintext = require_string(payload, "plaintext");
    encryption_dictionary = validate_encrypt_mapping(payload, plaintext);
    binary = PROJECT_ROOT / "encrypt";
    if not binary.is_file():
        raise RuntimeError("encrypt is not built. Run 'make encrypt' from the project root.");

    command = [str(binary), plaintext];
    if encryption_dictionary is not None:
        command.append(encryption_dictionary);

    with tempfile.TemporaryDirectory(prefix="crackuccino-") as temp_directory:
        completed = subprocess.run(
            command,
            cwd=temp_directory,
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        );
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or "encrypt failed.");
    return parse_encrypt_output(plaintext, completed.stdout);


def validate_mapping(input_dictionary, encryption_dictionary):
    """
    Validate positional dictionaries for a known decryption mapping.
    """

    if not re.fullmatch(r"[a-z]+", input_dictionary) or not re.fullmatch(r"[a-z]+", encryption_dictionary):
        raise ValueError("Mapping dictionaries must use only lowercase letters a-z.");
    if len(set(encryption_dictionary)) != len(encryption_dictionary):
        raise ValueError("Cipher mapping cannot contain duplicate letters.");


def run_direct_decrypt(payload):
    """
    Invert a known encryption mapping without starting a brute-force run.
    """

    ciphertext = require_string(payload, "ciphertext");
    input_dictionary = require_string(payload, "inputDictionary", 26).lower();
    encryption_dictionary = require_string(payload, "encryptionDictionary", 26).lower();
    validate_mapping(input_dictionary, encryption_dictionary);

    inverse = dict(zip(encryption_dictionary, input_dictionary));
    plaintext = "".join(inverse.get(character.lower(), character) for character in ciphertext);
    return {
        "ciphertext": ciphertext,
        "inputDictionary": input_dictionary,
        "encryptionDictionary": encryption_dictionary,
        "plaintext": plaintext,
    };


def parse_summary(lines):
    """
    Read the stats block printed by the MPI decryptor.
    """

    summary = {};
    for line in lines:
        if ":" not in line:
            continue;
        key, value = line.split(":", 1);
        if key.strip() in {"ranks", "n_letters", "depth", "expected", "visited", "valid_hits", "runtime_sec"}:
            summary[key.strip()] = value.strip();
    return summary;


def mpi_failure_message(stderr):
    """
    Return a client-safe MPI failure message without leaking runtime stderr.
    """

    normalized = stderr.lower();
    if any(marker in normalized for marker in MPI_NETWORK_ERROR_MARKERS):
        return "MPI decryption could not start in this server environment.";
    return "MPI decryption failed. Check server logs with the run id.";


def validate_decrypt_payload(payload):
    """
    Validate and normalize one MPI run request.
    """

    ciphertext = require_string(payload, "ciphertext");
    plaintext = payload.get("plaintext", "");
    dictionary = payload.get("dictionary");
    ranks = payload.get("ranks");
    depth = payload.get("depth");

    custom_dictionary = None;
    if isinstance(dictionary, str) and dictionary in ALLOWED_DICTIONARIES:
        pass;
    elif isinstance(dictionary, str) and dictionary.startswith(CUSTOM_DICTIONARY_PREFIX):
        custom_dictionary = validate_custom_dictionary_payload(payload, dictionary);
    else:
        raise ValueError("dictionary must be one of the built-in dictionaries or a valid uploaded dictionary.");
    if not isinstance(ranks, int) or isinstance(ranks, bool) or ranks < 1 or ranks > 16:
        raise ValueError("ranks must be an integer from 1 to 16.");
    if depth is not None and (not isinstance(depth, int) or isinstance(depth, bool) or depth < 1 or depth > 8):
        raise ValueError("depth must be null or an integer from 1 to 8.");
    if not isinstance(plaintext, str) or len(plaintext) > MAX_TEXT_LENGTH:
        raise ValueError(f"plaintext must be a string with at most {MAX_TEXT_LENGTH} characters.");
    request = {
        "ciphertext": ciphertext,
        "plaintext": plaintext.strip(),
        "dictionary": dictionary,
        "ranks": ranks,
        "depth": None if ranks == 1 else depth,
    };
    letters = unique_letters(ciphertext);
    expected = math.factorial(len(letters)) if len(letters) <= 20 else 0;
    if expected == 0 or expected > MAX_BRUTE_FORCE_PERMUTATIONS:
        raise ValueError(
            f"Brute-force input is too large ({len(letters)} unique letters, {expected:,} permutations). "
            f"Use at most 12 unique letters or decrypt with a known mapping."
        );
    return request, custom_dictionary;


def unique_letters(value):
    """
    Return unique ASCII letters in first-seen order.
    """

    return "".join(dict.fromkeys(character for character in value.lower() if "a" <= character <= "z"));


def automatic_depth(nletters, ranks):
    """
    Match the native decryptor's automatic prefix-depth selection.
    """

    if nletters <= 1:
        return 0;
    target = 1;
    ways = nletters;
    while ways < ranks * 8 and target < nletters:
        target += 1;
        ways *= nletters - target + 1;
    return min(target, nletters - 1);


def prefix_task_count(nletters, depth):
    """
    Return the number of ordered prefix tasks for one MPI workload.
    """

    total = 1;
    for offset in range(depth):
        total *= nletters - offset;
    return total;


def assigned_task_count(total_tasks, ranks, rank):
    """
    Return the exact round-robin prefix-task count assigned to one rank.
    """

    if rank >= total_tasks:
        return 0;
    return ((total_tasks - 1 - rank) // ranks) + 1;


def initial_rank_statuses(ranks, nletters, depth):
    """
    Build queued status records before MPI creates progress files.
    """

    total_tasks = prefix_task_count(nletters, depth);
    permutations_per_task = math.factorial(nletters - depth);
    return [
        {
            "rank": rank,
            "state": "queued",
            "permutations": 0,
            "hits": 0,
            "prefix": "",
            "samplePermutation": "",
            "permutationTrace": [],
            "completedTasks": 0,
            "assignedTasks": assigned_task_count(total_tasks, ranks, rank),
            "assignedPermutations": assigned_task_count(total_tasks, ranks, rank) * permutations_per_task,
        }
        for rank in range(ranks)
    ];


def append_permutation_trace(previous, sample, visited, prefix):
    """
    Append one sampled candidate for a rank, retaining a bounded trace.
    """

    trace = previous.get("permutationTrace");
    if not isinstance(trace, list):
        trace = [];
    else:
        trace = [
            item for item in trace
            if isinstance(item, dict)
            and isinstance(item.get("permutation"), str)
            and isinstance(item.get("visited"), int)
            and isinstance(item.get("prefix"), str)
        ];

    if sample and (not trace or trace[-1]["permutation"] != sample or trace[-1]["visited"] != visited):
        trace.append({
            "permutation": sample,
            "visited": visited,
            "prefix": prefix,
        });

    return trace[-MAX_PERMUTATION_TRACE_PER_RANK:];


def read_rank_trace(progress_directory, rank, previous):
    """
    Read appended sampled permutations for one rank.
    """

    path = Path(progress_directory) / f"rank_{rank}_trace.jsonl";
    if not path.is_file():
        return append_permutation_trace(previous, "", 0, "");

    trace = [];
    try:
        with path.open("r", encoding="utf-8") as file:
            for line in file:
                try:
                    value = json.loads(line);
                    permutation = value.get("permutation");
                    visited = value.get("visited");
                    prefix = value.get("prefix", "");
                    if not isinstance(permutation, str) or not isinstance(visited, int) or not isinstance(prefix, str):
                        continue;
                    if trace and trace[-1]["permutation"] == permutation and trace[-1]["visited"] == visited:
                        continue;
                    trace.append({
                        "permutation": permutation,
                        "visited": visited,
                        "prefix": prefix,
                    });
                    if len(trace) > MAX_PERMUTATION_TRACE_PER_RANK:
                        trace = trace[-MAX_PERMUTATION_TRACE_PER_RANK:];
                except (TypeError, json.JSONDecodeError):
                    continue;
    except OSError:
        return append_permutation_trace(previous, "", 0, "");
    return trace;


def read_rank_statuses(progress_directory, fallback):
    """
    Read the latest structured progress snapshot written by each rank.
    """

    statuses = [];
    for previous in fallback:
        path = Path(progress_directory) / f"rank_{previous['rank']}.json";
        if not path.is_file():
            statuses.append(previous);
            continue;
        try:
            value = json.loads(path.read_text(encoding="utf-8"));
            rank = int(value["rank"]);
            sample = str(value.get("samplePermutation", ""));
            visited = int(value["permutations"]);
            prefix = str(value["prefix"]);
            trace = read_rank_trace(progress_directory, rank, previous);
            if sample:
                trace = append_permutation_trace({"permutationTrace": trace}, sample, visited, prefix);
            statuses.append({
                "rank": rank,
                "state": str(value["state"]),
                "permutations": visited,
                "hits": int(value["hits"]),
                "prefix": prefix,
                "samplePermutation": sample,
                "permutationTrace": trace,
                "completedTasks": int(value["completedTasks"]),
                "assignedTasks": previous["assignedTasks"],
                "assignedPermutations": previous["assignedPermutations"],
            });
        except (KeyError, TypeError, ValueError, json.JSONDecodeError, OSError):
            statuses.append(previous);
    return statuses;


def update_live_stats(run, started):
    """
    Refresh aggregate metrics from the latest per-rank snapshots.
    """

    visited = sum(status["permutations"] for status in run["rankStatuses"]);
    expected = run["stats"]["expectedPermutations"];
    run["stats"]["visitedPermutations"] = visited;
    run["stats"]["validHits"] = sum(status["hits"] for status in run["rankStatuses"]);
    run["stats"]["runtimeSeconds"] = time.perf_counter() - started;
    run["stats"]["progressPercent"] = min(100, visited / expected * 100) if expected > 0 else 0;


def run_snapshot(run):
    """
    Return a copy safe for JSON serialization outside the run lock.
    """

    return {
        "id": run["id"],
        "state": run["state"],
        "createdAt": run["createdAt"],
        "completedAt": run["completedAt"],
        "request": dict(run["request"]),
        "rankStatuses": [dict(status) for status in run["rankStatuses"]],
        "results": [dict(result) for result in run["results"]],
        "stats": dict(run["stats"]),
        "error": run["error"],
    };


def sign_session_id(session_id):
    """
    Sign one opaque browser-session id so clients cannot forge ownership.
    """

    signature = hmac.new(SESSION_SECRET, session_id.encode("ascii"), hashlib.sha256).hexdigest();
    return f"{session_id}.{signature}";


def read_session_id(cookie_header):
    """
    Read and verify the opaque session cookie sent by one browser profile.
    """

    if not cookie_header:
        return None;
    cookie = SimpleCookie();
    try:
        cookie.load(cookie_header);
    except CookieError:
        return None;
    morsel = cookie.get(SESSION_COOKIE_NAME);
    if morsel is None:
        return None;
    try:
        session_id, signature = morsel.value.rsplit(".", 1);
    except ValueError:
        return None;
    expected = hmac.new(SESSION_SECRET, session_id.encode("ascii"), hashlib.sha256).hexdigest();
    if not hmac.compare_digest(signature, expected):
        return None;
    return session_id;


def owned_run(run_id, session_id):
    """
    Return a run only when it belongs to the requesting browser session.
    """

    run = RUNS.get(run_id);
    if run is None or run["ownerSession"] != session_id or run["deleted"]:
        return None;
    return run;


def prune_session_history(session_id):
    """
    Keep only the newest bounded terminal history for one browser session.
    """

    visible = [
        run for run in RUNS.values()
        if run["ownerSession"] == session_id and not run["deleted"]
    ];
    terminal = sorted(
        (run for run in visible if run["state"] not in {"queued", "running"}),
        key=lambda run: run["createdAt"],
    );
    while len(visible) > MAX_RUN_HISTORY and terminal:
        oldest = terminal.pop(0);
        RUNS.pop(oldest["id"], None);
        visible.remove(oldest);


def terminate_process(process):
    """
    Stop an MPI process group gracefully, then force kill if needed.
    """

    if process is None or process.poll() is not None:
        return;
    try:
        os.killpg(os.getpgid(process.pid), signal.SIGTERM);
    except (ProcessLookupError, PermissionError, OSError):
        try:
            process.terminate();
        except OSError:
            return;
    try:
        process.wait(timeout=PROCESS_TERMINATE_GRACE_SECONDS);
        return;
    except subprocess.TimeoutExpired:
        pass;

    try:
        os.killpg(os.getpgid(process.pid), signal.SIGKILL);
    except (ProcessLookupError, PermissionError, OSError):
        try:
            process.kill();
        except OSError:
            return;
    try:
        process.wait(timeout=PROCESS_TERMINATE_GRACE_SECONDS);
    except subprocess.TimeoutExpired:
        return;


def request_run_cancel(run_id, session_id, reason="MPI run canceled by user."):
    """
    Mark one run for cancellation and stop its process if already running.
    """

    process = None;
    with RUNS_LOCK:
        run = owned_run(run_id, session_id);
        if run is None:
            security_log("run_cancel_rejected", reason="not_found");
            raise KeyError(run_id);
        run["lastSeen"] = time.time();
        if run["state"] in {"complete", "failed", "canceled"}:
            return run_snapshot(run);
        run["cancelRequested"] = True;
        run["cancelReason"] = reason;
        process = run.get("process");
        snapshot = run_snapshot(run);

    terminate_process(process);
    return snapshot;


def request_run_delete(run_id, session_id):
    """
    Hide one owned run immediately and stop active MPI work before cleanup.
    """

    process = None;
    with RUNS_LOCK:
        run = owned_run(run_id, session_id);
        if run is None:
            security_log("run_delete_rejected", reason="not_found");
            raise KeyError(run_id);
        if run["state"] in {"queued", "running"}:
            run["deleted"] = True;
            run["cancelRequested"] = True;
            run["cancelReason"] = "Deleted by user; MPI run was canceled.";
            process = run.get("process");
        else:
            RUNS.pop(run_id, None);

    terminate_process(process);


def disconnect_session(session_id):
    """
    Stop active MPI work owned by a browser session that closed or navigated away.
    """

    processes = [];
    with RUNS_LOCK:
        for run in RUNS.values():
            if run["ownerSession"] == session_id and not run["deleted"] and run["state"] in {"queued", "running"}:
                run["cancelRequested"] = True;
                run["cancelReason"] = "Browser disconnected; MPI run was canceled.";
                processes.append(run.get("process"));
    for process in processes:
        terminate_process(process);


def cancel_active_runs(reason):
    """
    Cancel every active MPI run, used during server shutdown.
    """

    processes = [];
    with RUNS_LOCK:
        for run in RUNS.values():
            if run["state"] in {"queued", "running"}:
                run["cancelRequested"] = True;
                run["cancelReason"] = reason;
                processes.append(run.get("process"));
    for process in processes:
        terminate_process(process);


def execute_run(run_id):
    """
    Run serial or MPI decryption in a worker thread.
    """

    with RUNS_LOCK:
        run = RUNS.get(run_id);
        if run is None:
            return;
        request = dict(run["request"]);
        custom_dictionary = run.get("customDictionary");

    serial = request["ranks"] == 1;
    binary = PROJECT_ROOT / ("decrypt-serial" if serial else "decrypt-mpi");
    mpi = None if serial else shutil.which("mpirun");
    if not binary.is_file() or (not serial and mpi is None):
        security_log("mpi_failure", runId=run_id, reason="runtime_unavailable");
        with RUNS_LOCK:
            run["state"] = "failed";
            run["error"] = (
                "Serial runtime is unavailable. Build decrypt-serial."
                if serial else
                "MPI runtime is unavailable. Install MPI and build decrypt-mpi."
            );
            run["completedAt"] = time.time();
        return;

    with tempfile.TemporaryDirectory(prefix="crackuccino-") as temp_directory:
        ciphertext_path = Path(temp_directory) / "ciphertext.txt";
        progress_directory = Path(temp_directory) / "progress";
        progress_directory.mkdir();
        ciphertext_path.write_text(request["ciphertext"] + "\n", encoding="utf-8");
        if custom_dictionary is not None:
            resolved_dictionary = Path(temp_directory) / "custom_dictionary.txt";
            resolved_dictionary.write_text("\n".join(custom_dictionary["words"]) + "\n", encoding="utf-8");
        else:
            resolved_dictionary = dictionary_path(request["dictionary"]);
            if resolved_dictionary is None:
                with RUNS_LOCK:
                    run["state"] = "failed";
                    run["error"] = "Dictionary is no longer available.";
                    run["completedAt"] = time.time();
                return;
        command = (
            [str(binary), str(ciphertext_path), str(resolved_dictionary), "-s"]
            if serial else
            [
                mpi, "-np", str(request["ranks"]), str(binary), str(ciphertext_path),
                str(resolved_dictionary), "-s", "--progress-dir", str(progress_directory),
            ]
        );
        if not serial and request["depth"] is not None:
            command.extend(["-d", str(request["depth"])]);

        with RUNS_LOCK:
            run["state"] = "running";
            if serial and run["rankStatuses"]:
                run["rankStatuses"][0]["state"] = "running";

        started = time.perf_counter();
        stdout_path = Path(temp_directory) / "stdout.txt";
        stderr_path = Path(temp_directory) / "stderr.txt";
        with stdout_path.open("w", encoding="utf-8") as stdout_file, stderr_path.open("w", encoding="utf-8") as stderr_file:
            process = subprocess.Popen(command, stdout=stdout_file, stderr=stderr_file, text=True, start_new_session=True);
            with RUNS_LOCK:
                run["process"] = process;
            cancel_reason = None;
            while process.poll() is None:
                should_cancel = False;
                with RUNS_LOCK:
                    if time.time() - run["lastSeen"] > STALE_CLIENT_SECONDS:
                        run["cancelRequested"] = True;
                        run["cancelReason"] = "Browser session stopped; MPI run was canceled.";
                    should_cancel = run["cancelRequested"];
                    cancel_reason = run["cancelReason"];
                    if not serial:
                        run["rankStatuses"] = read_rank_statuses(progress_directory, run["rankStatuses"]);
                    update_live_stats(run, started);
                if should_cancel:
                    terminate_process(process);
                    break;
                time.sleep(0.2);
            returncode = process.returncode;
        stdout = stdout_path.read_text(encoding="utf-8");
        stderr = stderr_path.read_text(encoding="utf-8");

        with RUNS_LOCK:
            if not serial:
                run["rankStatuses"] = read_rank_statuses(progress_directory, run["rankStatuses"]);
            run["process"] = None;

    with RUNS_LOCK:
        if run["cancelRequested"]:
            for status in run["rankStatuses"]:
                if status["state"] != "complete":
                    status["state"] = "canceled";
            run["state"] = "canceled";
            run["error"] = run["cancelReason"] or cancel_reason or "MPI run canceled.";
            run["completedAt"] = time.time();
            if run["deleted"]:
                RUNS.pop(run_id, None);
            update_live_stats(run, started);
            return;

    if returncode != 0:
        security_log(
            "native_failure",
            runId=run_id,
            returncode=returncode,
            stderr=stderr.strip()[:1000],
        );
        with RUNS_LOCK:
            run["state"] = "failed";
            run["error"] = "Serial decryption failed. Check server logs with the run id." if serial else mpi_failure_message(stderr);
            run["completedAt"] = time.time();
        return;

    lines = [line.strip() for line in stdout.splitlines() if line.strip()];
    summary = parse_summary(lines);
    results = [];
    for line in lines:
        match = RESULT_PATTERN.match(line);
        if match:
            rank = int(match.group(1));
            results.append({"rank": rank, "permutation": match.group(2), "plaintext": match.group(3)});

    visited = int(summary.get("visited", "0"));
    runtime = float(summary.get("runtime_sec", "0"));
    serial_estimate = runtime if serial else runtime * request["ranks"];
    speedup = serial_estimate / runtime if runtime > 0 else 0;
    with RUNS_LOCK:
        if serial and run["rankStatuses"]:
            run["rankStatuses"][0].update({
                "state": "complete",
                "permutations": visited,
                "hits": int(summary.get("valid_hits", "0")),
                "completedTasks": run["rankStatuses"][0]["assignedTasks"],
            });
        run["state"] = "complete";
        run["completedAt"] = time.time();
        run["results"] = results;
        run["stats"].update({
            "ranks": request["ranks"],
            "uniqueLetters": int(summary.get("n_letters", "0")),
            "depth": int(summary.get("depth", "0")),
            "expectedPermutations": int(summary.get("expected", "0")),
            "visitedPermutations": visited,
            "validHits": int(summary.get("valid_hits", "0")),
            "runtimeSeconds": runtime,
            "serialEstimateSeconds": serial_estimate,
            "speedup": speedup,
            "efficiencyPercent": (speedup / request["ranks"] * 100) if request["ranks"] else 0,
            "progressPercent": min(100, visited / int(summary.get("expected", "0")) * 100)
            if int(summary.get("expected", "0")) > 0 else 0,
        });
        if run["deleted"]:
            RUNS.pop(run_id, None);


def start_run(payload, session_id):
    """
    Create one asynchronous serial or MPI run and return its initial snapshot.
    """

    request, custom_dictionary = validate_decrypt_payload(payload);
    letters = unique_letters(request["ciphertext"]);
    depth = 1 if request["ranks"] == 1 else (
        request["depth"] if request["depth"] is not None else automatic_depth(len(letters), request["ranks"])
    );
    expected = math.factorial(len(letters)) if len(letters) <= 20 else 0;
    run_id = uuid.uuid4().hex[:12];
    run = {
        "id": run_id,
        "state": "queued",
        "createdAt": time.time(),
        "completedAt": None,
        "request": request,
        "customDictionary": custom_dictionary,
        "rankStatuses": initial_rank_statuses(request["ranks"], len(letters), depth),
        "results": [],
        "stats": {
            "ranks": request["ranks"],
            "uniqueLetters": len(letters),
            "depth": depth,
            "expectedPermutations": expected,
            "visitedPermutations": 0,
            "validHits": 0,
            "runtimeSeconds": 0,
            "serialEstimateSeconds": 0,
            "speedup": 0,
            "efficiencyPercent": 0,
            "progressPercent": 0,
        },
        "error": None,
        "process": None,
        "cancelRequested": False,
        "cancelReason": None,
        "lastSeen": time.time(),
        "ownerSession": session_id,
        "deleted": False,
    };
    with RUNS_LOCK:
        active_runs = [
            item for item in RUNS.values()
            if not item["deleted"] and item["state"] in {"queued", "running"}
        ];
        active_count = sum(
            1 for item in active_runs if item["ownerSession"] == session_id
        );
        if active_count >= MAX_ACTIVE_RUNS_PER_SESSION:
            security_log("run_limit_rejected", reason="session_active_limit");
            raise ValueError("This browser session already has an active MPI run. Cancel it before starting another.");
        if len(active_runs) >= MAX_ACTIVE_RUNS_GLOBAL:
            security_log("run_limit_rejected", reason="global_active_limit");
            raise RuntimeError("The local MPI worker limit is busy. Wait for an active run to finish.");
        RUNS[run_id] = run;
        prune_session_history(session_id);
    threading.Thread(target=execute_run, args=(run_id,), daemon=True).start();
    return run_snapshot(run);


class RequestHandler(BaseHTTPRequestHandler):
    """
    Serve the built React frontend and its local JSON API.
    """

    server_version = "CrackuccinoHTTP/1.0";

    def session_id(self):
        """
        Return this browser profile's verified session id, creating one if needed.
        """

        session_id = read_session_id(self.headers.get("Cookie"));
        if session_id is None:
            session_id = secrets.token_urlsafe(24);
            self.new_session_cookie = sign_session_id(session_id);
        return session_id;

    def client_ip(self):
        """
        Return the direct client IP seen by this server.
        """

        return str(getattr(self, "client_address", ("unknown",))[0] or "unknown");

    def require_same_origin(self):
        """
        Reject cross-origin state-changing requests that could reuse session cookies.
        """

        origin = self.headers.get("Origin");
        if origin is None:
            if is_production():
                security_log("origin_rejected", reason="missing", host=self.headers.get("Host"), clientIp=self.client_ip());
                raise PermissionError("Origin is required for state-changing requests.");
            return;
        parsed = urlparse(origin);
        if parsed.netloc != self.headers.get("Host") or parsed.scheme not in {"http", "https"}:
            security_log("origin_rejected", reason="mismatch", origin=origin, host=self.headers.get("Host"), clientIp=self.client_ip());
            raise PermissionError("Cross-origin requests are not allowed.");

    def send_security_headers(self):
        """
        Attach basic browser security headers to each response.
        """

        self.send_header("X-Content-Type-Options", "nosniff");
        self.send_header("X-Frame-Options", "SAMEORIGIN");
        self.send_header("Referrer-Policy", "no-referrer");
        self.send_header("Cross-Origin-Resource-Policy", "same-origin");
        self.send_header("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
        self.send_header(
            "Content-Security-Policy",
            "default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; "
            "script-src 'self'; frame-src 'self'; connect-src 'self'; object-src 'none'; base-uri 'none'",
        );
        self.send_header("Cache-Control", "no-store");
        if hasattr(self, "new_session_cookie"):
            cookie = f"{SESSION_COOKIE_NAME}={self.new_session_cookie}; Path=/; HttpOnly; SameSite=Strict";
            if is_production():
                cookie += "; Secure";
            self.send_header(
                "Set-Cookie",
                cookie,
            );

    def send_json(self, status, payload):
        """
        Serialize and send one JSON response.
        """

        data = json.dumps(payload).encode("utf-8");
        self.send_response(status);
        self.send_header("Content-Type", "application/json");
        self.send_header("Content-Length", str(len(data)));
        self.send_security_headers();
        self.end_headers();
        self.wfile.write(data);

    def read_json_body(self, max_bytes=MAX_BODY_BYTES):
        """
        Read a small JSON object from the request body.
        """

        request_path = urlparse(getattr(self, "path", "")).path;
        if not self.headers.get("Content-Type", "").lower().startswith("application/json"):
            security_log("invalid_json_request", reason="content_type", path=request_path, clientIp=self.client_ip());
            raise ValueError("Content-Type must be application/json.");
        try:
            length = int(self.headers.get("Content-Length", "0"));
        except ValueError as error:
            security_log("invalid_json_request", reason="content_length", path=request_path, clientIp=self.client_ip());
            raise ValueError("Content-Length must be an integer.") from error;
        if length <= 0 or length > max_bytes:
            security_log("oversized_json_request", limit=max_bytes, size=length, path=request_path, clientIp=self.client_ip());
            raise ValueError("Request body is missing or too large.");
        try:
            payload = json.loads(self.rfile.read(length).decode("utf-8"));
        except UnicodeDecodeError:
            security_log("invalid_json_request", reason="utf8", path=request_path, clientIp=self.client_ip());
            raise;
        if not isinstance(payload, dict):
            security_log("invalid_json_request", reason="non_object", path=request_path, clientIp=self.client_ip());
            raise ValueError("Request body must be a JSON object.");
        return payload;

    def serve_static(self, path):
        """
        Serve a Vite build file without allowing path traversal.
        """

        relative = "index.html" if path == "/" else unquote(path).lstrip("/");
        requested = (FRONTEND_ROOT / relative).resolve();
        if FRONTEND_ROOT not in requested.parents and requested != FRONTEND_ROOT:
            self.send_error(403);
            return;
        if not requested.is_file():
            if relative.lower().endswith((".pdf", ".png", ".jpg", ".jpeg", ".gif", ".webp", ".svg", ".ico", ".js", ".css", ".map")):
                self.send_error(404);
                return;
            requested = FRONTEND_ROOT / "index.html";
        if not requested.is_file():
            self.send_error(404, "Frontend is not built. Run npm run build in frontend.");
            return;
        data = requested.read_bytes();
        self.send_response(200);
        self.send_header("Content-Type", mimetypes.guess_type(requested.name)[0] or "application/octet-stream");
        self.send_header("Content-Length", str(len(data)));
        self.send_security_headers();
        self.end_headers();
        self.wfile.write(data);

    def do_GET(self):
        """
        Route health requests and frontend assets.
        """

        path = urlparse(self.path).path;
        session_id = self.session_id();
        if path == "/api/health":
            self.send_json(200, {
                "status": "ok",
                "encryptAvailable": (PROJECT_ROOT / "encrypt").is_file(),
                "mpiAvailable": shutil.which("mpirun") is not None and (PROJECT_ROOT / "decrypt-mpi").is_file(),
            });
            return;
        if path == "/api/dictionaries":
            self.send_json(200, {"dictionaries": list_dictionaries()});
            return;
        if path.startswith("/api/dictionaries/"):
            identifier = unquote(path.removeprefix("/api/dictionaries/"));
            try:
                self.send_json(200, read_dictionary_detail(identifier));
            except ValueError as error:
                self.send_json(404, {"error": str(error)});
            return;
        if path == "/api/runs":
            with RUNS_LOCK:
                runs = sorted(
                    (
                        run for run in RUNS.values()
                        if run["ownerSession"] == session_id and not run["deleted"]
                    ),
                    key=lambda run: run["createdAt"],
                    reverse=True,
                );
                self.send_json(200, {"runs": [run_snapshot(run) for run in runs]});
            return;
        if path.startswith("/api/runs/"):
            run_id = path.removeprefix("/api/runs/");
            with RUNS_LOCK:
                run = owned_run(run_id, session_id);
                if run is None:
                    self.send_json(404, {"error": "Run not found."});
                else:
                    if run["state"] in {"queued", "running"}:
                        run["lastSeen"] = time.time();
                    self.send_json(200, run_snapshot(run));
            return;
        if path.startswith("/api/"):
            self.send_json(404, {"error": "API endpoint not found."});
            return;
        self.serve_static(path);

    def do_POST(self):
        """
        Route encryption and decryption requests.
        """

        path = urlparse(self.path).path;
        try:
            self.require_same_origin();
            session_id = self.session_id();
            client_ip = self.client_ip();
            if path == "/api/dictionaries":
                check_rate_limit("dictionary_upload", session_id, client_ip);
                payload = self.read_json_body(MAX_DICTIONARY_BODY_BYTES);
                self.send_json(201, upload_dictionary(payload));
            elif path == "/api/session/disconnect":
                self.read_json_body();
                disconnect_session(session_id);
                self.send_json(200, {"disconnected": True});
            elif path.startswith("/api/runs/") and path.endswith("/cancel"):
                run_id = path.removeprefix("/api/runs/").removesuffix("/cancel");
                try:
                    self.send_json(200, request_run_cancel(run_id, session_id));
                except KeyError:
                    self.send_json(404, {"error": "Run not found."});
            else:
                payload = self.read_json_body();
                if path == "/api/encrypt":
                    check_rate_limit("encrypt", session_id, client_ip);
                    self.send_json(200, run_encrypt(payload));
                elif path == "/api/decrypt/direct":
                    check_rate_limit("direct_decrypt", session_id, client_ip);
                    self.send_json(200, run_direct_decrypt(payload));
                elif path == "/api/runs":
                    check_rate_limit("runs", session_id, client_ip);
                    self.send_json(202, start_run(payload, session_id));
                else:
                    self.send_json(404, {"error": "API endpoint not found."});
        except (ValueError, json.JSONDecodeError, UnicodeDecodeError) as error:
            if path == "/api/dictionaries":
                security_log("dictionary_upload_rejected", reason=error.__class__.__name__, clientIp=self.client_ip());
            self.send_json(400, {"error": str(error)});
        except PermissionError as error:
            self.send_json(403, {"error": str(error)});
        except subprocess.TimeoutExpired:
            self.send_json(504, {"error": "Cipher process timed out."});
        except RateLimitError as error:
            self.send_json(429, {"error": str(error)});
        except RuntimeError as error:
            self.send_json(503, {"error": str(error)});
        except BrokenPipeError:
            return;
        except Exception as error:
            print(f"Request failed: {error}");
            self.send_json(500, {"error": "Unexpected server error."});

    def do_DELETE(self):
        """
        Delete an owned history item and stop it first when still active.
        """

        path = urlparse(self.path).path;
        try:
            self.require_same_origin();
            session_id = self.session_id();
            if not path.startswith("/api/runs/"):
                self.send_json(404, {"error": "API endpoint not found."});
                return;
            run_id = path.removeprefix("/api/runs/");
            try:
                request_run_delete(run_id, session_id);
                self.send_json(200, {"deleted": True});
            except KeyError:
                self.send_json(404, {"error": "Run not found."});
        except PermissionError as error:
            self.send_json(403, {"error": str(error)});
        except BrokenPipeError:
            return;
        except Exception as error:
            print(f"Request failed: {error}");
            self.send_json(500, {"error": "Unexpected server error."});

    def log_message(self, format_string, *args):
        """
        Keep local request logs concise.
        """

        print(f"{self.address_string()} - {format_string % args}");


def main():
    """
    Parse CLI options and start the local web server.
    """

    parser = argparse.ArgumentParser(description="Run the Crackuccino web app.");
    parser.add_argument("--host", default="127.0.0.1");
    parser.add_argument("--port", type=int, default=8000);
    args = parser.parse_args();

    validate_production_security();
    validate_bind_safety(args.host);
    os.chdir(PROJECT_ROOT);
    server = ThreadingHTTPServer((args.host, args.port), RequestHandler);
    print(f"Crackuccino web app running at http://{args.host}:{args.port}");
    try:
        server.serve_forever();
    except KeyboardInterrupt:
        pass;
    finally:
        cancel_active_runs("Server stopped; MPI run was canceled.");
        server.server_close();


if __name__ == "__main__":
    main();
