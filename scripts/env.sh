# Source this to put the hermetic toolchain on PATH:
#   source scripts/env.sh
#
# Defaults to the shared toolchain at /home/armando/c-opt/toolchains, used by
# the sibling stb-image-opt and stb-image-write-opt forks; falls back to a
# per-project install under $XDG_DATA_HOME if that is not present.
if [ -z "${TOOLCHAIN_ROOT:-}" ]; then
    if [ -d /home/armando/c-opt/toolchains ]; then
        TOOLCHAIN_ROOT=/home/armando/c-opt/toolchains
    else
        TOOLCHAIN_ROOT=${XDG_DATA_HOME:-$HOME/.local/share}/stb-image-resize-opt/toolchain
    fi
fi
ZIG_VER=${ZIG_VER:-0.16.0}
LLVM_VER=${LLVM_VER:-23.1.2}
ISPC_VER=${ISPC_VER:-1.31.0}

for d in \
    "$TOOLCHAIN_ROOT/llvm-$LLVM_VER/bin" \
    "$TOOLCHAIN_ROOT/zig-$ZIG_VER" \
    "$TOOLCHAIN_ROOT/ispc-$ISPC_VER/bin" \
; do
    [ -d "$d" ] && PATH="$d:$PATH"
done
export PATH

# Vendored ICU 70 required by ld.lld from the official LLVM tarball.
ICU70_LIB="$TOOLCHAIN_ROOT/llvm-$LLVM_VER/icu70/usr/lib/x86_64-linux-gnu"
if [ -d "$ICU70_LIB" ]; then
    LD_LIBRARY_PATH="$ICU70_LIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH
fi

# valgrind (callgrind) ships as a plain prefix here; it needs VALGRIND_LIB to
# find its tool libraries when not installed at the compiled-in prefix.
command -v valgrind >/dev/null 2>&1 || {
    for v in "$TOOLCHAIN_ROOT"/valgrind-*/usr/bin; do
        [ -d "$v" ] && PATH="$PATH:$v"
    done
    export PATH
}
for vg in "$TOOLCHAIN_ROOT"/valgrind-*/usr/libexec/valgrind; do
    [ -d "$vg" ] && export VALGRIND_LIB="$vg"
done

command -v clang      >/dev/null && export CLANG=$(command -v clang)
command -v zig        >/dev/null && export ZIG=$(command -v zig)
command -v llvm-bolt  >/dev/null && export LLVM_BOLT=$(command -v llvm-bolt)
command -v valgrind   >/dev/null && export VALGRIND=$(command -v valgrind)
