#!/usr/bin/env python3
#
#    Copyright (c) 2026 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#
"""Build Matter nRF Connect SDK examples inside the CI Docker image.

The host only needs Docker. Inside ``chip-build-nrf-platform`` the script
mirrors ``examples-nrfconnect.yaml``: checkout submodules, bootstrap, then
``scripts/examples/nrfconnect_example.sh`` for each target. Optional HEX export
uses the image's ``mergehex.py`` (same as CI's bundled NCS Zephyr).
"""

from __future__ import annotations

import argparse
import fcntl
import os
import platform
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, Sequence

DEFAULT_DOCKER_IMAGE = "ghcr.io/project-chip/chip-build-nrf-platform:215"
DOCKER_START_TIMEOUT_SEC = 120
DOCKER_POLL_INTERVAL_SEC = 2
MATTER_ENVIRONMENT_DIRNAME = ".environment"
STALE_ENV_MARKER = ".environment-docker"
ZAP_INSTALL_DIRNAME = ".zap-install"

DOCKER_INSTALL_LINKS = {
    "Linux": "https://docs.docker.com/engine/install/",
    "Darwin": "https://docs.docker.com/desktop/setup/install/mac-install/",
    "Windows": "https://docs.docker.com/desktop/setup/install/windows-install/",
}

class BuildError(RuntimeError):
    pass


def _os_key() -> str:
    return platform.system()


def _find_executable(name: str) -> Optional[str]:
    return shutil.which(name)


def _require_tool(name: str, install_links: dict[str, str]) -> str:
    path = _find_executable(name)
    if path:
        return path
    link = install_links.get(_os_key(), install_links["Linux"])
    raise BuildError(
        f"{name} was not found on PATH.\n"
        f"Install it for {_os_key()} and ensure it is available in your shell:\n"
        f"  {link}"
    )


def _docker_daemon_running(docker: str) -> bool:
    result = subprocess.run(
        [docker, "info"],
        capture_output=True,
        text=True,
        check=False,
    )
    return result.returncode == 0


def _run_start_command(cmd: Sequence[str]) -> bool:
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return False
    if result.returncode == 0:
        print(f"Started Docker daemon: {' '.join(cmd)}", flush=True)
        return True
    return False


def _launch_detached(cmd: Sequence[str]) -> bool:
    try:
        subprocess.Popen(
            list(cmd),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
    except OSError:
        return False
    print(f"Launching Docker: {' '.join(cmd)}", flush=True)
    return True


def _start_docker_daemon_linux() -> None:
    if _run_start_command(["systemctl", "start", "docker"]):
        return
    if _run_start_command(["sudo", "-n", "systemctl", "start", "docker"]):
        return
    if _run_start_command(["sudo", "systemctl", "start", "docker"]):
        return
    if _run_start_command(["sudo", "service", "docker", "start"]):
        return
    if _run_start_command(["systemctl", "--user", "start", "docker-desktop"]):
        return

    for app in (
        "/opt/docker-desktop/bin/docker-desktop",
        "/usr/bin/docker-desktop",
    ):
        if Path(app).is_file() and _launch_detached([app]):
            return

    raise BuildError(
        "Docker daemon is not running and could not be started automatically on Linux.\n"
        "Start it manually, for example:\n"
        "  sudo systemctl start docker\n"
        "  open Docker Desktop"
    )


def _start_docker_daemon_darwin() -> None:
    if Path("/Applications/Docker.app").is_dir():
        if _launch_detached(["open", "-a", "Docker"]):
            return

    raise BuildError(
        "Docker daemon is not running and could not be started automatically on macOS.\n"
        "Start Docker Desktop from Applications and retry."
    )


def _start_docker_daemon_windows() -> None:
    program_files = os.environ.get("ProgramFiles", r"C:\Program Files")
    local_app_data = os.environ.get("LocalAppData", "")
    desktop_paths = [
        Path(program_files) / "Docker" / "Docker" / "Docker Desktop.exe",
        Path(local_app_data) / "Docker" / "Docker Desktop.exe",
    ]
    for desktop_path in desktop_paths:
        if desktop_path.is_file() and _launch_detached([str(desktop_path)]):
            return

    if _run_start_command(["net", "start", "com.docker.service"]):
        return

    raise BuildError(
        "Docker daemon is not running and could not be started automatically on Windows.\n"
        "Start Docker Desktop from the Start menu and retry."
    )


def _start_docker_daemon() -> None:
    system = _os_key()
    if system == "Linux":
        _start_docker_daemon_linux()
    elif system == "Darwin":
        _start_docker_daemon_darwin()
    elif system == "Windows":
        _start_docker_daemon_windows()
    else:
        raise BuildError(f"Automatic Docker startup is not supported on {system}.")


def _ensure_docker_daemon(
    docker: str,
    *,
    timeout_sec: int = DOCKER_START_TIMEOUT_SEC,
) -> None:
    if _docker_daemon_running(docker):
        return

    print("Docker daemon is not running. Attempting to start it...", flush=True)
    _start_docker_daemon()

    deadline = time.monotonic() + timeout_sec
    while time.monotonic() < deadline:
        if _docker_daemon_running(docker):
            print("Docker daemon is ready.", flush=True)
            return
        time.sleep(DOCKER_POLL_INTERVAL_SEC)

    raise BuildError(
        f"Docker daemon did not become ready within {timeout_sec} seconds.\n"
        "Start Docker manually and retry."
    )


def _run(
    cmd: Sequence[str],
    *,
    cwd: Optional[Path] = None,
    env: Optional[dict[str, str]] = None,
    check: bool = True,
) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(cmd), flush=True)
    result = subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        env=env,
        text=True,
        check=False,
    )
    if check and result.returncode != 0:
        raise BuildError(f"Command failed with exit code {result.returncode}: {' '.join(cmd)}")
    return result


def _chip_root(path: str) -> Path:
    root = Path(path).expanduser().resolve()
    if not (root / "scripts/examples/nrfconnect_example.sh").is_file():
        raise BuildError(f"{root} does not look like a connectedhomeip checkout.")
    return root


def _expand_list(values: Sequence[str]) -> list[str]:
    items: list[str] = []
    for value in values:
        for part in value.split(","):
            part = part.strip()
            if part:
                items.append(part)
    if not items:
        raise BuildError("At least one value is required.")
    return items


def _board_slug(board: str) -> str:
    return board.replace("/", "_")


def _iter_build_targets(examples: Sequence[str], boards: Sequence[str]) -> list[tuple[str, str]]:
    return [(example, board) for example in examples for board in boards]


@dataclass(frozen=True)
class FlashArtifact:
    example: str
    board: str
    hex_path: Path


def _flash_command(hex_path: Path) -> str:
    return (
        f"nrfutil device program --firmware {shlex.quote(str(hex_path))} "
        "--options chip_erase_mode=ERASE_ALL"
    )


_FLASH_BANNER_WIDTH = 80


def _print_flash_summary(artifacts: Sequence[FlashArtifact]) -> None:
    if not artifacts:
        return

    rule = "=" * _FLASH_BANNER_WIDTH
    print(f"\n{rule}", flush=True)
    print("  FLASH FIRMWARE — copy and run on the host (requires nrfutil + J-Link)", flush=True)
    print(rule, flush=True)

    for artifact in artifacts:
        print(f"\n  {artifact.example} / {artifact.board}:", flush=True)
        print(f"    {_flash_command(artifact.hex_path)}", flush=True)
        if _is_multicore_nrf5340(artifact.board):
            print(
                "    # Merged HEX includes application and network cores for nRF5340.",
                flush=True,
            )

    print("\n  Notes:", flush=True)
    print("  - With multiple kits connected, add: --serial-number <SN>", flush=True)
    print(f"{rule}\n", flush=True)


def _hex_basename(example: str, board: str, name: Optional[str], *, multi_target: bool) -> str:
    if name:
        if multi_target:
            return f"matter_{example}_{_board_slug(board)}_{name}"
        return f"matter_{example}_{name}"
    if multi_target:
        return f"{example}_{_board_slug(board)}"
    return example


def _list_examples(chip_root: Path) -> list[str]:
    examples_dir = chip_root / "examples"
    apps = sorted(
        path.parent.name
        for path in examples_dir.glob("*/nrfconnect/CMakeLists.txt")
    )
    return apps


def _build_dir(chip_root: Path, example: str) -> Path:
    return chip_root / "examples" / example / "nrfconnect" / "build"


def _build_dir_relpath(example: str) -> str:
    return f"examples/{example}/nrfconnect/build"


def _build_lock_path(chip_root: Path, example: str) -> Path:
    return chip_root / "examples" / example / "nrfconnect" / ".nrf_build.lock"


def _acquire_build_lock(chip_root: Path, example: str):
    """Hold an exclusive lock so two Docker builds cannot share one build tree."""
    if _os_key() == "Windows":
        return None

    lock_path = _build_lock_path(chip_root, example)
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    lock_file = lock_path.open("w", encoding="utf-8")
    try:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError as exc:
        lock_file.close()
        raise BuildError(
            f"Another nrf_build.py run is already building {example} "
            f"(lock file: {lock_path}). Wait for it to finish."
        ) from exc
    lock_file.write(f"pid={os.getpid()}\n")
    lock_file.flush()
    return lock_file


def _docker_exec_script(chip_root: Path, docker_image: str, script: str) -> None:
    """Run a bash script as root inside a throwaway container on the mounted checkout."""
    docker = _require_tool("docker", DOCKER_INSTALL_LINKS)
    _ensure_docker_daemon(docker)
    docker_home = chip_root / ".docker-build-home"
    docker_home.mkdir(exist_ok=True)
    inner = f"set -eo pipefail && mkdir -p \"$HOME\" && {script}"
    _run(
        [
            docker,
            "run",
            "--rm",
            "-v",
            f"{chip_root}:{chip_root}",
            "-w",
            str(chip_root),
            "-e",
            f"HOME={docker_home}",
            docker_image,
            "bash",
            "-lc",
            inner,
        ]
    )


def _reclaim_docker_created_paths(
    chip_root: Path,
    relative_paths: Sequence[str],
    docker_image: str,
) -> None:
    """Fix host ownership of paths created as root inside Docker bind mounts."""
    if _os_key() == "Windows":
        return

    existing = [path for path in relative_paths if (chip_root / path).exists()]
    if not existing:
        return

    uid = os.getuid()
    gid = os.getgid()
    script = " && ".join(f"chown -R {uid}:{gid} {shlex.quote(path)}" for path in existing)
    print("Reclaiming host ownership of Docker-created paths...", flush=True)
    _docker_exec_script(chip_root, docker_image, script)


def _docker_remove_build_dir_command(example: str) -> str:
    return f"rm -rf {shlex.quote(_build_dir_relpath(example))}"


def _build_dir_references_stale_environment(chip_root: Path, example: str) -> bool:
    """Return True when a prior build cached paths under the old .environment-docker layout."""
    build_dir = _build_dir(chip_root, example)
    if not build_dir.is_dir():
        return False
    result = subprocess.run(
        ["grep", "-r", "-q", "-m", "1", STALE_ENV_MARKER, str(build_dir)],
        capture_output=True,
        check=False,
    )
    return result.returncode == 0


def _is_multicore_nrf5340(board: str) -> bool:
    return "nrf5340" in board.lower()


def _ensure_host_ownership(path: Path) -> None:
    if _os_key() == "Windows":
        os.chmod(path, 0o755 if path.is_dir() else 0o644)
        return

    uid = os.getuid()
    gid = os.getgid()
    os.chown(path, uid, gid)
    os.chmod(path, 0o755 if path.is_dir() else 0o644)


def _prepare_output_dir(output_dir: Path) -> None:
    if output_dir.exists() and not os.access(output_dir, os.W_OK):
        raise BuildError(
            f"Output directory {output_dir} is not writable (often caused by a prior Docker run as root).\n"
            f"Reclaim it with:\n"
            f"  sudo chown -R {os.getuid()}:{os.getgid()} {output_dir}"
        )

    try:
        output_dir.mkdir(parents=True, exist_ok=True)
    except PermissionError as exc:
        parent = output_dir.parent
        raise BuildError(
            f"Cannot create output directory {output_dir}: {exc}\n"
            f"If a parent directory is root-owned, reclaim it with:\n"
            f"  sudo chown -R {os.getuid()}:{os.getgid()} {parent}"
        ) from exc

    _ensure_host_ownership(output_dir)


def _netcore_merge_hex(build_dir: Path) -> Path:
    ipc_radio_hex = build_dir / "signed_by_mcuboot_and_b0_ipc_radio.hex"
    if ipc_radio_hex.is_file():
        return ipc_radio_hex

    matches = sorted(build_dir.glob("signed_by_mcuboot_and_b0_*.hex"))
    for candidate in matches:
        name = candidate.name
        if name in {
            "signed_by_mcuboot_and_b0_mcuboot.hex",
            "signed_by_mcuboot_and_b0_nrfconnect.hex",
        }:
            continue
        if "ipc_radio" in name or "cpunet" in name:
            return candidate

    raise BuildError(
        f"No network-core HEX found under {build_dir}. "
        "Expected signed_by_mcuboot_and_b0_ipc_radio.hex for nRF5340 builds."
    )


def _merge_hex_inputs(build_dir: Path, board: str) -> list[Path]:
    inputs = [
        build_dir / "nrfconnect" / "zephyr" / "zephyr.signed.hex",
        build_dir / "mcuboot" / "zephyr" / "zephyr.hex",
    ]
    if _is_multicore_nrf5340(board):
        inputs.append(_netcore_merge_hex(build_dir))
    return inputs


def _snippet_west_args(debug: bool, diagnostics_logs: bool) -> list[str]:
    snippets: list[str] = []
    if debug:
        snippets.append("matter-debug")
    if diagnostics_logs:
        snippets.append("diagnostics-logs")
    if not snippets:
        return []
    return [f'-Dnrfconnect_SNIPPET={";".join(snippets)}']


def _matter_environment_dir(chip_root: Path) -> Path:
    return chip_root / MATTER_ENVIRONMENT_DIRNAME


def _zap_install_dir(chip_root: Path) -> Path:
    return chip_root / ZAP_INSTALL_DIRNAME


def _docker_checkout_submodules_step(skip_submodules: bool) -> str:
    if skip_submodules:
        return "true"
    return (
        "python3 scripts/checkout_submodules.py "
        "--allow-changing-global-git-config --shallow --platform nrfconnect"
    )


def _docker_prepare_environment_step() -> str:
    # Mirrors CI bootstrap, but also drops host-platform .environment / .zap-install
    # left on the bind-mounted checkout (common on macOS).
    return " && ".join(
        [
            (
                "if [ -d .zap-install ] && ! .zap-install/zap-cli --version >/dev/null 2>&1; "
                "then rm -rf .zap-install; fi"
            ),
            (
                "if [ ! -s .environment/activate.sh ] || "
                "! .environment/cipd/packages/pigweed/bin/ninja --version >/dev/null 2>&1; "
                "then "
                "rm -rf .environment .zap-install; "
                "unset PYTHONPATH PYTHONHOME; "
                "PW_ENVSETUP_NO_BANNER=1 PW_NO_CIPD_CACHE_DIR=1 "
                "source scripts/bootstrap.sh -p all,nrfconnect; "
                "fi"
            ),
        ]
    )


def _output_hex_path(
    output_dir: Path,
    example: str,
    board: str,
    name: Optional[str],
    *,
    multi_target: bool,
) -> Path:
    base_name = _hex_basename(example, board, name, multi_target=multi_target)
    return output_dir / f"{base_name}.hex"


def _docker_mergehex_step(
    chip_root: Path,
    example: str,
    board: str,
    output_hex: Path,
) -> str:
    build_dir = _build_dir(chip_root, example)
    inputs = _merge_hex_inputs(build_dir, board)
    quoted_inputs = " ".join(shlex.quote(str(path)) for path in inputs)
    return (
        f"python3 \"$ZEPHYR_BASE/scripts/build/mergehex.py\" "
        f"-o {shlex.quote(str(output_hex))} {quoted_inputs}"
    )


def _docker_volume_args(chip_root: Path, output_dir: Optional[Path]) -> list[str]:
    args = ["-v", f"{chip_root}:{chip_root}"]
    if output_dir is None:
        return args
    resolved = output_dir.resolve()
    try:
        resolved.relative_to(chip_root)
    except ValueError:
        args.extend(["-v", f"{resolved}:{resolved}"])
    return args


def _docker_inner_script(
    chip_root: Path,
    targets: Sequence[tuple[str, str]],
    extra_west_args: Sequence[str],
    *,
    skip_submodules: bool,
    pristine: bool,
    output_dir: Optional[Path],
    name: Optional[str],
    multi_target: bool,
) -> str:
    steps = [
        "set -eo pipefail",
        "mkdir -p \"$HOME\"",
        "git config --global --add safe.directory '*'",
        _docker_checkout_submodules_step(skip_submodules),
        _docker_prepare_environment_step(),
    ]

    pristine_examples: set[str] = set()
    for example, board in targets:
        step_parts: list[str] = []
        stale_build = _build_dir_references_stale_environment(chip_root, example)
        if example not in pristine_examples and (pristine or stale_build):
            step_parts.append(_docker_remove_build_dir_command(example))
            pristine_examples.add(example)
        step_parts.append(_build_command(chip_root, example, board, extra_west_args))
        steps.append(" && ".join(step_parts))

    if output_dir is not None:
        resolved_output = output_dir.resolve()
        steps.append(f"mkdir -p {shlex.quote(str(resolved_output))}")
        for example, board in targets:
            output_hex = _output_hex_path(
                resolved_output,
                example,
                board,
                name,
                multi_target=multi_target,
            )
            steps.append(_docker_mergehex_step(chip_root, example, board, output_hex))
            steps.append(f"echo Created merged HEX {shlex.quote(str(output_hex))}")

    return " && ".join(steps)


def _build_command(
    chip_root: Path,
    example: str,
    board: str,
    extra_west_args: Sequence[str],
) -> str:
    quoted_args = " ".join(shlex.quote(arg) for arg in extra_west_args)
    suffix = f" {quoted_args}" if quoted_args else ""
    return f"scripts/examples/nrfconnect_example.sh {example} {board}{suffix}"


def _collect_flash_artifacts(
    output_dir: Path,
    targets: Sequence[tuple[str, str]],
    name: Optional[str],
    *,
    multi_target: bool,
) -> list[FlashArtifact]:
    artifacts: list[FlashArtifact] = []
    missing: list[Path] = []
    for example, board in targets:
        hex_path = _output_hex_path(
            output_dir,
            example,
            board,
            name,
            multi_target=multi_target,
        )
        if hex_path.is_file():
            artifacts.append(FlashArtifact(example=example, board=board, hex_path=hex_path))
        else:
            missing.append(hex_path)
    if missing:
        missing_list = "\n  ".join(str(path) for path in missing)
        raise BuildError(f"Expected merged HEX output was not created:\n  {missing_list}")
    return artifacts


def _validate_example(chip_root: Path, example: str) -> None:
    cmake = chip_root / "examples" / example / "nrfconnect" / "CMakeLists.txt"
    if not cmake.is_file():
        known = ", ".join(_list_examples(chip_root))
        raise BuildError(f"Unknown example {example!r}. Available examples: {known}")


def _validate_build_targets(chip_root: Path, examples: Sequence[str], boards: Sequence[str]) -> list[tuple[str, str]]:
    if not examples:
        raise BuildError("At least one --example is required.")
    if not boards:
        raise BuildError("At least one --board is required.")

    for example in examples:
        _validate_example(chip_root, example)

    return _iter_build_targets(examples, boards)


def _resolve_build_plan(args: argparse.Namespace) -> tuple[list[tuple[str, str]], list[str], bool, Optional[Path]]:
    examples = _expand_list(args.example)
    boards = _expand_list(args.board)
    chip_root = _chip_root(args.chip_root)
    targets = _validate_build_targets(chip_root, examples, boards)
    multi_target = len(targets) > 1

    extra_west_args = _snippet_west_args(args.debug, args.diagnostics_logs)
    extra_west_args.extend(args.extra_west_arg)

    output_dir: Optional[Path] = None
    if args.output_dir:
        output_dir = Path(args.output_dir).expanduser().resolve()

    return targets, extra_west_args, multi_target, output_dir


def build(args: argparse.Namespace) -> None:
    chip_root = _chip_root(args.chip_root)
    targets, extra_west_args, multi_target, output_dir = _resolve_build_plan(args)

    docker = _require_tool("docker", DOCKER_INSTALL_LINKS)
    _ensure_docker_daemon(docker)

    docker_home = chip_root / ".docker-build-home"
    docker_home.mkdir(exist_ok=True)

    if output_dir is not None:
        _prepare_output_dir(output_dir)

    matter_env = _matter_environment_dir(chip_root)
    inner = _docker_inner_script(
        chip_root,
        targets,
        extra_west_args,
        skip_submodules=args.skip_submodules,
        pristine=args.pristine,
        output_dir=output_dir,
        name=args.name,
        multi_target=multi_target,
    )

    docker_cmd = [
        "docker",
        "run",
        "--rm",
        *_docker_volume_args(chip_root, output_dir),
        "-w",
        str(chip_root),
        "-e",
        f"HOME={docker_home}",
        args.docker_image,
        "bash",
        "-lc",
        inner,
    ]

    build_locks: list[object] = []
    try:
        for example in {example for example, _ in targets}:
            lock = _acquire_build_lock(chip_root, example)
            if lock is not None:
                build_locks.append(lock)
        print("Running checkout, bootstrap, and build inside Docker (same order as CI)...", flush=True)
        _run(docker_cmd)
    finally:
        for lock in build_locks:
            lock.close()
        reclaim_paths = [_build_dir_relpath(example) for example in {example for example, _ in targets}]
        if matter_env.exists():
            reclaim_paths.append(MATTER_ENVIRONMENT_DIRNAME)
        zap_install = _zap_install_dir(chip_root)
        if zap_install.exists():
            reclaim_paths.append(ZAP_INSTALL_DIRNAME)
        _reclaim_docker_created_paths(chip_root, reclaim_paths, args.docker_image)

    if output_dir is not None:
        flash_artifacts = _collect_flash_artifacts(
            output_dir,
            targets,
            args.name,
            multi_target=multi_target,
        )
        for artifact in flash_artifacts:
            _ensure_host_ownership(artifact.hex_path)
        _print_flash_summary(flash_artifacts)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Build Matter nRF Connect SDK examples inside chip-build-nrf-platform.",
    )
    parser.add_argument(
        "chip_root",
        help="Path to the connectedhomeip repository checkout.",
    )
    parser.add_argument(
        "--example",
        required=True,
        nargs="+",
        metavar="EXAMPLE",
        help="One or more example application names. Comma-separated values are also accepted.",
    )
    parser.add_argument(
        "--board",
        required=True,
        nargs="+",
        metavar="BOARD",
        help="One or more Zephyr board targets. Comma-separated values are also accepted.",
    )
    parser.add_argument(
        "--output-dir",
        help="Directory where renamed HEX artifacts are copied after a successful build.",
    )
    parser.add_argument(
        "--name",
        help="Optional suffix for output filenames. "
        "When set: matter_<example>_<name>.hex (or matter_<example>_<board>_<name>.hex "
        "for multiple targets). When omitted: <example>.hex.",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help='Add -Dnrfconnect_SNIPPET="matter-debug" to the west build.',
    )
    parser.add_argument(
        "--diagnostics-logs",
        action="store_true",
        help='Add -Dnrfconnect_SNIPPET="diagnostics-logs" to the west build.',
    )
    parser.add_argument(
        "--extra-west-arg",
        action="append",
        default=[],
        metavar="ARG",
        help="Additional west/cmake argument passed after '--'. Can be repeated.",
    )
    parser.add_argument(
        "--skip-submodules",
        action="store_true",
        help="Skip nrfconnect submodule checkout.",
    )
    parser.add_argument(
        "--pristine",
        action="store_true",
        help="Remove examples/<example>/nrfconnect/build before building that example.",
    )
    parser.add_argument(
        "--docker-image",
        default=DEFAULT_DOCKER_IMAGE,
        help=f"Docker image to use (default: {DEFAULT_DOCKER_IMAGE}).",
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = _build_parser().parse_args(argv)

    try:
        build(args)
    except BuildError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
