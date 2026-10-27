"""Remove the machine name and absolute executable path from a Google Benchmark JSON file.

The CPU model, cache sizes and OS stay (they are the point of recording the machine); the
host name and the local directory layout do not belong in a public repository.

    python python/scrub_bench_json.py docs/results/bench.json
"""
import json
import sys
from pathlib import Path


def main() -> None:
    path = Path(sys.argv[1])
    data = json.loads(path.read_text(encoding="utf-8"))
    data["context"]["host_name"] = "redacted"
    data["context"]["executable"] = "build/bench/quietroom_bench"
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
