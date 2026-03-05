set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

build_root := "build"
debug_dir := "{{build_root}}/debug"
release_dir := "{{build_root}}/release"
dev_dir := "{{build_root}}/dev"
debug_clang_dir := "build/debug-clang"
android_dir := "{{build_root}}/android"
wasm_dir := "{{build_root}}/wasm"

default: help

help:
  @echo "Recipes:"
  @echo "  debug     Configure + build Debug in {{debug_dir}}"
  @echo "  release   Configure + build Release in {{release_dir}}"
  @echo "  dev       Configure + build RelWithDebInfo in {{dev_dir}}"
  @echo "  debug-clang Configure + build Clang Debug in {{debug_clang_dir}}"
  @echo "  test      Run ctest with debug preset"
  @echo "  check     Clang debug quality gate for bot/runtime touched flows"
  @echo "  tidy      Run cached clang-tidy on changed files (uses {{debug_dir}})"
  @echo "  fmt       Run clang-format on changed C/C++ files"
  @echo "  android   Run scripts/deploy.sh android with BUILD_DIR={{android_dir}}"
  @echo "  wasm      Run scripts/deploy.sh wasm with BUILD_DIR={{wasm_dir}}"
  @echo "  clean     Remove {{build_root}}"

debug:
  cmake --preset debug
  cmake --build --preset debug

release:
  cmake --preset release
  cmake --build --preset release

dev:
  cmake --preset dev
  cmake --build --preset dev

debug-clang:
  cmake --preset debug-clang
  cmake --build --preset debug-clang

test:
  ctest --preset debug

check:
  cmake --preset debug-clang
  cmake --build --preset debug-clang --target adapter-tests adapter-ui-dispatch-tests adapter-ui-command-tests adapter-bot-config-tests engine-adapter-tests
  env QT_QPA_PLATFORM=offscreen ./{{debug_clang_dir}}/adapter-tests
  env QT_QPA_PLATFORM=offscreen ./{{debug_clang_dir}}/adapter-ui-dispatch-tests
  env QT_QPA_PLATFORM=offscreen ./{{debug_clang_dir}}/adapter-ui-command-tests
  env QT_QPA_PLATFORM=offscreen ./{{debug_clang_dir}}/adapter-bot-config-tests
  env QT_QPA_PLATFORM=offscreen ./{{debug_clang_dir}}/engine-adapter-tests

tidy:
  ./scripts/dev.sh clang-tidy {{debug_dir}}

fmt:
  @files="$$(git diff --name-only -- '*.cpp' '*.cc' '*.cxx' '*.h' '*.hpp')"; \
    if [ -z "$$files" ]; then \
      echo "[format] no changed C/C++ files"; \
    else \
      echo "$$files" | xargs clang-format -i; \
    fi

android:
  BUILD_DIR={{android_dir}} ./scripts/deploy.sh android

wasm:
  BUILD_DIR={{wasm_dir}} ./scripts/deploy.sh wasm

clean:
  rm -rf {{build_root}}
