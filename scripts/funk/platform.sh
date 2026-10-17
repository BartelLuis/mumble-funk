#!/usr/bin/env bash
# Shared allowlist; arguments come from /etc/os-release in the installer.
funk_require_supported_os() {
    case "${1:-}:${2:-}" in
        ubuntu:24.04|ubuntu:26.04) return 0 ;;
        *)
            printf 'Unterstützt werden Ubuntu 24.04 LTS und 26.04 LTS; erkannt: ID=%s, VERSION_ID=%s.\n' \
                "${1:-unbekannt}" "${2:-unbekannt}" >&2
            return 1
            ;;
    esac
}
