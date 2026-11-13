#!/bin/sh

DEVICE=$1

verify_partition() {
    part=$1
    expected=$2
    excluded=$3
    [ -n "$part" ] || { echo "usage: verify_partition DEV [EXPECTED [EXCLUDED]]" >&2; return 2; }

    mnt=$(mktemp -d) || return 2
    if ! sudo mount -o ro "$part" "$mnt"; then
        rmdir "$mnt"
        return 2
    fi

    echo "Content of $part:"
    (cd "$mnt"; find \( -name lost+found \) -prune -o -print)

    rc=0
    nl='
'
    old_ifs=$IFS
    set -f # no globbing while splitting
    IFS=$nl

    for p in $expected; do
        rel=${p#"${p%%[!/]*}"} # strip leading slashes
        if [ -e "$mnt/$rel" ]; then
            echo "OK       $p"
        else
            echo "MISSING  $p"
            rc=1
        fi
    done

    for p in $excluded; do
        rel=${p#"${p%%[!/]*}"}
        if [ -e "$mnt/$rel" ]; then
            echo "UNWANTED $p"
            rc=1
        else
            echo "OK       $p (absent)"
        fi
    done

    IFS=$old_ifs
    set +f
    sudo umount "$mnt"
    rmdir "$mnt"
    return $rc
}

verify_partition ${DEVICE}p2 \
"foo/bar/buz/dozzer.yaml
foo/bar/buz/buzzer.yaml
foo/bar/settings.cfg
foo/test.c
nested/one/two/three/four.txt" \
"empty/" || exit $?

verify_partition ${DEVICE}p3 \
"foo/bar/buz/dozzer.yaml
foo/bar/buz/buzzer.yaml
foo/bar/settings.cfg
foo/test.c" \
"empty/
nested/" || exit $?

verify_partition ${DEVICE}p4 \
"foo/bar/buz/dozzer.yaml
foo/bar/buz/buzzer.yaml
foo/bar/settings.cfg
nested/one/two/three/four.txt" \
"empty/
foo/test.c" || exit $?
