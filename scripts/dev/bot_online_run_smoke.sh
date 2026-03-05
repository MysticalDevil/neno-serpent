#!/usr/bin/env bash
set -euo pipefail

# Purpose: smoke test bot-online-run lifecycle (trainer starts and is cleaned up).

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_ROOT="${NENOSERPENT_TMP_DIR:-${NENOSERPENT_CACHE_DIR:-${ROOT_DIR}/cache/dev}}"
WORKSPACE="${TMP_ROOT}/bot_online_run_smoke"
MOCK_DEV_SH="${WORKSPACE}/mock-dev.sh"
MOCK_LOG="${WORKSPACE}/mock.log"
TRAINER_PID_FILE="${WORKSPACE}/trainer.pid"
TRAINER_TERM_FILE="${WORKSPACE}/trainer.term"

mkdir -p "${WORKSPACE}"
rm -f "${MOCK_LOG}" "${TRAINER_PID_FILE}" "${TRAINER_TERM_FILE}"

cat > "${MOCK_DEV_SH}" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
CMD="${1:-}"
shift || true
MOCK_LOG_FILE="${MOCK_LOG_FILE:?missing MOCK_LOG_FILE}"
TRAINER_PID_FILE="${TRAINER_PID_FILE:?missing TRAINER_PID_FILE}"
TRAINER_TERM_FILE="${TRAINER_TERM_FILE:?missing TRAINER_TERM_FILE}"
printf '%s %s\n' "${CMD}" "$*" >> "${MOCK_LOG_FILE}"
case "${CMD}" in
  bot-online-train)
    echo "$$" > "${TRAINER_PID_FILE}"
    trap 'echo term > "${TRAINER_TERM_FILE}"; exit 0' TERM INT
    while true; do
      sleep 1
    done
    ;;
  bot-run)
    sleep 1
    exit 0
    ;;
  *)
    echo "mock unknown command: ${CMD}" >&2
    exit 1
    ;;
esac
EOF
chmod +x "${MOCK_DEV_SH}"

MOCK_LOG_FILE="${MOCK_LOG}" \
TRAINER_PID_FILE="${TRAINER_PID_FILE}" \
TRAINER_TERM_FILE="${TRAINER_TERM_FILE}" \
NENOSERPENT_DEV_SCRIPT="${MOCK_DEV_SH}" \
timeout 20s "${ROOT_DIR}/scripts/dev/bot_online_run.sh" \
  --workspace "${WORKSPACE}" \
  --ui-mode screen

if ! rg -q '^bot-online-train ' "${MOCK_LOG}"; then
  echo "[bot-online-run-smoke] missing bot-online-train invocation" >&2
  exit 1
fi
if ! rg -q '^bot-run ' "${MOCK_LOG}"; then
  echo "[bot-online-run-smoke] missing bot-run invocation" >&2
  exit 1
fi
if [[ ! -f "${TRAINER_TERM_FILE}" ]]; then
  echo "[bot-online-run-smoke] trainer was not terminated on exit" >&2
  exit 1
fi

echo "[bot-online-run-smoke] passed"
