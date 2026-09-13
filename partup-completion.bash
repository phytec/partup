#/usr/bin/env bash

partup_commands="install package show version"
help_options="-h --help"
app_options="-q --quiet -d --debug -D --debug-domains"
install_options="-s --skip-checksums"
package_options="-f --force -C, --directory"
show_options="-s --size"

function exists_in_list()
{
	LIST=$1
	DELIMITER=$2
	VALUE=$3
	[[ "$LIST" =~ ($DELIMITER|^)$VALUE($DELIMITER|$) ]]
}

function add_command_help()
{
	if [[ ${previous} == ${command} ]] ; then
		# only add help if we are first after command
		COMPREPLY+=( $(compgen -W "${help_options}" -- ${current}) )
	fi
	if exists_in_list "${help_options}" " " "${previous}" ; then
		# if we have choosen help option, nothing should follow
		return 0
	fi
}


_partup_completions()
{
	local current=""
	local previous=""
	local command=""

	if [[ ${COMP_CWORD} > 0 ]] ; then
		current="${COMP_WORDS[COMP_CWORD]}"

		if [[ ${COMP_CWORD} > 1 ]] ; then
			# iterate over all COMP_WORDS
			for s in "${COMP_WORDS[@]:1}"; do
				if exists_in_list "${partup_commands}" " " "${s}" ; then
					command="${s}"
				fi
				((i++))
			done
			previous="${COMP_WORDS[COMP_CWORD - 1]}"
		fi
	fi

	if [[ -z ${command} ]] ; then
		COMPREPLY=( $(compgen -W "${help_options} ${partup_commands} ${app_options}"  -- ${current}) )
		return 0
	fi

	# we may have things like "partup --debug install ..."

	if [[ ${command} == install ]] ; then
		add_command_help ${command} ${current} ${previous}

		COMPREPLY+=( $(compgen -W "${install_options} ${app_options}" -- ${current}) )

		# PACKAGE
		COMPREPLY+=( $(compgen -f -o plusdirs -X '!*.partup' -- ${current}) )

		# DEVICE -> if previous ends with partup, complete only from /dev
		if [[ ${previous} == *.partup ]] ; then
			if [[ -z ${current} ]] ; then
				COMPREPLY=( '/dev' )
				return 0
			else
				COMPREPLY+=( $(compgen -f -o plusdirs -- ${current} ) )
			fi
		fi

		# don't allow options after dev
		if [[ ${previous} == /dev/* ]] ; then
			COMPREPLY=()
		fi

		return 0
	fi

	if [[ ${command} == package ]] ; then
		add_command_help ${command} ${current} ${previous}

		COMPREPLY+=( $(compgen -W "${package_options} ${app_options}" -- ${current}) )

		# -C, --directory=DIR
		# PACKAGE FILES…

		if exists_in_list "${package_options}" " " ${previous} ; then
			#if output is previous, a file must follow
			COMPREPLY=( $(compgen -f -- ${current}) )
		fi

		return 0
	fi

	if [[ ${command} == show ]] ; then
		add_command_help ${command} ${current} ${previous}

		COMPREPLY+=( $(compgen -W "${show_options} ${app_options}" -- ${current}) )

		# PACKAGE - if we do not already have one, complete available partup files
		if [[ ${previous} != *.partup ]] ; then			
			COMPREPLY+=( $(compgen -f -o plusdirs -X '!*.partup' -- ${current}) )
		fi

		return 0
	fi

	if [[ ${command} == version ]] ; then
		COMPREPLY=()
		return 0
	fi

	return 1
}

complete -o filenames -F _partup_completions partup
