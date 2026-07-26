import subprocess
import sys
import pathlib

# root dir
ROOT = pathlib.Path(__file__).resolve().parent;
TESTS_DIR = ROOT / "tests" / "python";

# list of test files to run
tests = [f"t{i}.py" for i in range(1, 13)];

for test in tests:
    path = TESTS_DIR / test;

    print(f"\n{'*' * 100}");
    print(f"Running {test} ...");
    print(f"{'*' * 100}\n");

    try:
        subprocess.run(
            [sys.executable, str(path)],
            check=True
        );
        print(f"\n{test} completed successfully.\n");

    except subprocess.CalledProcessError as e:
        print(f"\n{test} failed with exit code {e.returncode}.\n");

    except FileNotFoundError:
        print(f"\nSkipping {test}: not found.\n");
