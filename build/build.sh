#!/usr/bin/zsh
MV=$(which mv)
ZIP=$(which zip)
TAR=$(which tar)

function makeAll() {
  for os in $@; do
		cd $os; make &; wait; cd -
  done
}

function compress() {
  PATH="../../"
  for os in $@; do
		cd $os
		case $os in
		  win32) $ZIP -r awit_windows32.zip awit.exe ${PATH}README.md ${PATH}mga\ halimbawa &
             wait
             $MV awit_windows32.zip ../
			;;
		  win64) $ZIP -r awit_windows64.zip awit.exe ${PATH}README.md ${PATH}mga\ halimbawa &
             wait
             $MV awit_windows64.zip ../
			;;
		  linux) $TAR -cf awit_linux.tar.gz awit ${PATH}README.md ${PATH}mga\ halimbawa &
             wait
             $MV awit_linux.tar.gz ../
			;;
	  esac
		cd -
  done
}

# Prompt message is in LIGHT BLUE.
LBLUE='\033[1;34m'
NC='\033[0m' # No Color

# Error codes
E2BIG=7
EINVAL=22
E2SMALL=64

function usage() {
    NAME="build"
    printf "${LBLUE}USAGE:${NC}\n"
    printf "    ./$NAME -h or --help\n"
    printf "        Displays this message.\n"
    printf "    ./$NAME version [1.0.1]\n"
    printf "        Creates the appropriate build files.\n"
    printf "    ./$NAME os [win32, win64, linux]\n"
    printf "        Builds AWIt executable for specific OS.\n"
    printf "        The only supported os are WINDOWS 32, WINDOWS 64, and Linux.\n\n"
    printf "${LBLUE}RETURN CODES:${NC}\n"
    printf "    $NAME has the following return codes:\n"
    printf "    0   success\n"
    printf "    $E2BIG   Argument list too long\n"
    printf "    $E2SMALL  Argument list too short\n"
}

# This script requires exactly one argument. If less are provided it prints
# the USAGE instructions
if [ ${#} -lt 1 ] ; then
    printf "${LBLUE}NO ARGUMENT FOUND${NC}\nExpects 1 argument.\n\n"
    usage
    exit $E2SMALL 
fi

if [ ${#} -gt 1 ] ; then
    printf "${LBLUE}TOO MANY ARGUMENTS FOUND${NC}\nExpects 1 argument.\n\n"
    usage
    exit $E2BIG
fi

if [ "${1}" = "-h" ] || [ "${1}" = "--help" ] ; then
    usage
    exit 0
fi

if [ "${1}" = "win32" ] || [ "${1}" = "win64" ] || [ "${1}" = "linux" ] ; then
    printf "${LBLUE}Compiling...${NC}\n"
    makeAll ${1}

    printf "${LBLUE}\nCompressing...${NC}\n"
    compress ${1}

    printf "${LBLUE}Done!${NC}\n"
    exit 0
fi

printf "${LBLUE}Compiling...${NC}\n"
makeAll linux win32 win64

printf "${LBLUE}\nCompressing...${NC}\n"
compress linux win32 win64

printf "${LBLUE}\nDone! You can now update your release assets.\n"
printf "${NC}https://github.com/philiks/AWIT/releases/tag/${1}\n"
