# Shared differential matrix. Sourced by harness/diff.sh and
# harness/diff_dispatch.sh.
#
# Expects: O (oracle binary), C (candidate binary), BUILD, ROOT, QUICK, QUIET.
# Defines cmp_case() and run_matrix().

fail=0; n=0; nfail=0

# cmp_case <description> <args...>
cmp_case() {
    local desc=$1; shift
    "$O" "$@" > "$BUILD/_o.bin" 2>/dev/null; local ro=$?
    "$C" "$@" > "$BUILD/_c.bin" 2>/dev/null; local rc=$?
    n=$((n + 1))
    if [ "$ro" -ge 128 ] || [ "$rc" -ge 128 ]; then
        echo "CRASH [$desc] oracle_rc=$ro cand_rc=$rc: $*" >> "$BUILD/crashes.txt"
    fi
    if [ "$ro" != "$rc" ] || ! cmp -s "$BUILD/_o.bin" "$BUILD/_c.bin"; then
        [ "$QUIET" = 1 ] || echo "DIFF [$desc] oracle_rc=$ro cand_rc=$rc: $*"
        fail=1; nfail=$((nfail + 1))
    fi
}

run_matrix() {
    TYPES="uint8 uint8_srgb uint16 float half"
    LAYOUTS="1ch 2ch rgb 4ch rgba bgra argb abgr ra ar rgba_pm bgra_pm ra_pm ar_pm"
    EDGES="clamp reflect zero wrap"
    FILTERS="box triangle cubic catmullrom mitchell point other"
    if [ "$QUICK" = 1 ]; then
        TYPES="uint8 float"
        LAYOUTS="1ch rgb 4ch rgba rgba_pm"
        EDGES="clamp wrap"
        FILTERS="triangle catmullrom mitchell point"
    fi

    # matrix_core <input> <out-dim> <tag>
    matrix_core() {
        local input=$1 out=$2 tag=$3
        local type layout edge filter
        for type in $TYPES; do
            for layout in $LAYOUTS; do
                for edge in $EDGES; do
                    for filter in $FILTERS; do
                        cmp_case "$tag $type $layout $edge $filter" "$input" --out "$out" \
                            --in-type "$type" --in-layout "$layout" \
                            --edge "$edge" --filter "$filter" --quiet
                    done
                done
            done
        done
    }

    matrix_core "gen:plasma" 200x150 "up"
    matrix_core "gen:grad"   31x23  "down"
    matrix_core "gen:random" 64x48  "same"
    if [ "$QUICK" != 1 ]; then
        matrix_core "gen:flat" 1x1 "tiny1"
    fi

    for dims in 1x1 1x7 7x1 3x5 17x33; do
        for type in uint8 float; do
            cmp_case "tiny $dims $type" "gen:plasma" --in-dims "$dims" --out 5x5 \
                --in-type "$type" --in-layout rgba --quiet
            cmp_case "tiny-up $dims $type" "gen:plasma" --in-dims "$dims" --out 40x37 \
                --in-type "$type" --in-layout rgba --quiet
        done
    done

    for pair in "uint8 float" "float uint8" "uint16 uint8" "uint8 uint16" \
                "half float" "float half" "uint8_srgb float" "float uint8_srgb"; do
        set -- $pair
        cmp_case "mixed $1->$2" "gen:plasma" --out 80x60 \
            --in-type "$1" --out-type "$2" --in-layout rgba --out-layout rgba --quiet
    done

    if [ "$QUICK" != 1 ]; then
        for in_st in 0 7 13; do
            for out_st in 0 3 11; do
                cmp_case "stride $in_st/$out_st" "gen:plasma" --in-dims 64x48 --out 100x75 \
                    --in-type uint8 --in-layout rgba --stride-in $((64*4+in_st)) \
                    --stride-out $((100*4+out_st)) --quiet
            done
        done
    fi

    if [ "$QUICK" != 1 ]; then
        cmp_case "in-subrect"  "gen:plasma" --in-dims 64x48 --out 80x60 --in-type uint8 --in-layout rgba \
            --in-subrect 0.1 0.2 0.85 0.9 --quiet
        cmp_case "out-subrect" "gen:plasma" --in-dims 64x48 --out 80x60 --in-type uint8 --in-layout rgba \
            --out-subrect 5 7 40 30 --quiet
        cmp_case "both-subrect" "gen:plasma" --in-dims 64x48 --out 80x60 --in-type float --in-layout rgba \
            --in-subrect 0.05 0.05 0.6 0.6 --out-subrect 2 3 50 40 --quiet
    fi

    for sp in 2 3 4 7; do
        for type in uint8 float; do
            cmp_case "splits $sp $type" "gen:plasma" --in-dims 128x96 --out 300x200 \
                --in-type "$type" --in-layout rgba --splits "$sp" --quiet
        done
    done

    for la in rgba bgra argb abgr ra ar; do
        for fa in 0 1; do
            cmp_case "alpha $la fast$fa" "gen:plasma" --in-dims 64x48 --out 90x70 \
                --in-type uint8 --in-layout "$la" --fast-alpha "$fa" --quiet
        done
    done

    if [ "$QUICK" != 1 ]; then
        for rel in corpus/plasma_512.png corpus/grad_640_480.png \
                   corpus/grad_640_480_gray.png upstream/tests/pngsuite/primary/basn2c16.png \
                   upstream/tests/pngsuite/primary/basn0g01.png; do
            [ -e "$ROOT/$rel" ] || continue
            for layout in 1ch rgb 4ch rgba bgra ra; do
                for edge in clamp reflect zero wrap; do
                    for filter in triangle catmullrom mitchell point; do
                        cmp_case "real $rel $layout $edge $filter" "$ROOT/$rel" --out 200x130 \
                            --in-layout "$layout" --edge "$edge" --filter "$filter" --quiet
                    done
                done
            done
        done
    fi
}

matrix_summary() {
    if [ "$fail" -eq 0 ]; then
        echo "PASS: $n checks byte-exact"
    else
        echo "FAIL: $nfail of $n checks diverged"
    fi
    return $fail
}
