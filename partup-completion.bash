#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Bash completion for partup

_partup_commands="install package show version"
_partup_help_options="-h --help"
_partup_app_options="-q --quiet -d --debug -D --debug-domains"
_partup_install_options="-s --skip-checksums"
_partup_package_options="-f --force -C --directory"
_partup_show_options="-s --size"

_partup_in_list()
{
    local list=$1
    local value=$2

    [[ " ${list} " == *" ${value} "* ]]
}

_partup_complete_files()
{
    local pattern=$1
    local current=$2

    if [[ -n ${pattern} ]]; then
        mapfile -t -O "${#COMPREPLY[@]}" COMPREPLY < <(compgen -f -X "!${pattern}" -- "${current}")
        mapfile -t -O "${#COMPREPLY[@]}" COMPREPLY < <(compgen -d -- "${current}")
    else
        mapfile -t -O "${#COMPREPLY[@]}" COMPREPLY < <(compgen -f -- "${current}")
    fi
}

_partup_completions()
{
    local current="${COMP_WORDS[COMP_CWORD]}"
    local previous=""
    local command=""
    local -a positionals=()
    local i word skip=0

    COMPREPLY=()

    if (( COMP_CWORD > 0 )); then
        previous="${COMP_WORDS[COMP_CWORD - 1]}"
    fi

    # Find the command and the positional arguments in front of the cursor.
    # Arguments of options that take a value are skipped.
    for (( i = 1; i < COMP_CWORD; i++ )); do
        word="${COMP_WORDS[i]}"

        if (( skip )); then
            skip=0
            continue
        fi

        case "${word}" in
            -D|--debug-domains|-C|--directory)
                skip=1
                continue
                ;;
            -*)
                continue
                ;;
        esac

        if [[ -z ${command} ]] && _partup_in_list "${_partup_commands}" "${word}"; then
            command="${word}"
        else
            positionals+=( "${word}" )
        fi
    done

    # Complete the argument of an option that takes a value.
    case "${previous}" in
        -D|--debug-domains)
            return 0
            ;;
        -C|--directory)
            if [[ ${command} == package ]]; then
                mapfile -t COMPREPLY < <(compgen -d -- "${current}")
                return 0
            fi
            ;;
    esac

    case "${current}" in
        --debug-domains=*)
            return 0
            ;;
        --directory=*)
            if [[ ${command} == package ]]; then
                mapfile -t COMPREPLY < <(compgen -d -P "--directory=" -- "${current#--directory=}")
                return 0
            fi
            ;;
    esac

    if [[ -z ${command} ]]; then
        mapfile -t COMPREPLY < <(compgen -W "${_partup_help_options} ${_partup_commands} ${_partup_app_options}" -- "${current}")
        return 0
    fi

    local options="${_partup_app_options}"

    # Only offer help directly after the command
    if (( ${#positionals[@]} == 0 )) && [[ ${previous} == "${command}" ]]; then
        options+=" ${_partup_help_options}"
    fi

    case "${command}" in
        install)
            options+=" ${_partup_install_options}"
            ;;
        package)
            options+=" ${_partup_package_options}"
            ;;
        show)
            options+=" ${_partup_show_options}"
            ;;
        version)
            options="${_partup_help_options}"
            ;;
    esac

    if [[ ${current} == -* ]]; then
        mapfile -t COMPREPLY < <(compgen -W "${options}" -- "${current}")
        return 0
    fi

    case "${command}" in
        install)
            # install PACKAGE DEVICE
            case ${#positionals[@]} in
                0)
                    _partup_complete_files "*.partup" "${current}"
                    ;;
                1)
                    mapfile -t COMPREPLY < <(compgen -f -- "${current:-/dev/}")
                    ;;
            esac
            ;;
        package)
            # package PACKAGE FILES...
            if (( ${#positionals[@]} == 0 )); then
                _partup_complete_files "*.partup" "${current}"
            else
                _partup_complete_files "" "${current}"
            fi
            ;;
        show)
            # show PACKAGE
            if (( ${#positionals[@]} == 0 )); then
                _partup_complete_files "*.partup" "${current}"
            fi
            ;;
    esac

    if [[ -z ${current} ]] && (( ${#COMPREPLY[@]} == 0 )) && [[ ${command} != version ]]; then
        mapfile -t COMPREPLY < <(compgen -W "${options}" -- "${current}")
    fi

    return 0
}

complete -o filenames -F _partup_completions partup
