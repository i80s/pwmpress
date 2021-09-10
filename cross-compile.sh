#!/bin/bash

#
# Script for "cheating" your shell environment by setting
# PATH with a directory that contains "fake" GCC utilities
# (gcc, g++, ld, cc, ...) symbolic-linked to OpenWrt cross
# compilers.
# In this way developers can simply run "gcc", "g++"
# commands to cross-compile C/C++ programs, or just run
# "make" to build simple Makefile-based C/C++ projects.
#
# Please run './cross-compile.sh' for usage.
#
# Author: Justin Liu <rssnsj@gmail.com>
#

# By default we cheat local gcc, g++ commands by adding directory
# containing cross compilers to the head of $PATH.
PATH_CHEATING=Y
SETUP_TOOLCHAIN=Y

BINDIR=.fakebin.o
LOGFILE=$BINDIR/links.log

get_toolchain_dir()
{
	local dir
	for dir in "$1"/staging_dir/toolchain-*; do
		if [ -f $dir/include/stdio.h ]; then
			basename $dir
			return 0
		fi
	done
}
get_target_dir()
{
	local dir
	for dir in "$1"/staging_dir/target-*; do
		if [ -d $dir/usr ]; then
			basename $dir
			return 0
		fi
	done
}

file_is_script()
{
	local file="$1"
	[ -x "$file" ] || return 1
	local file_type=`file -Lbi "$file"`
	case "$file_type" in
		text/*) return 0;;
	esac
	return 1
}

setup_toolchain()
{ (
	cd $openwrt_root || exit 1

	echo "Setting up the toolchain ..." >&2

	mkdir -p $BINDIR

	# Link: gcc -> mips-openwrt-linux-gcc
	local prog
	for prog in addr2line ar as c++ c++filt cpp elfedit g++ gcc gcc-4.6.4 gcov gdb \
			gprof ld ld.bfd nm objcopy objdump ranlib readelf size strings strip; do
		[ -e $BINDIR/$prog ] && continue || :
		local cross_bin=`basename staging_dir/$toolchain_dir/bin/*-openwrt-linux-$prog`
		[ -e staging_dir/$toolchain_dir/bin/*-openwrt-linux-$prog ] || continue
		if file_is_script staging_dir/$toolchain_dir/bin/*-openwrt-linux-$prog; then
			rm -f $BINDIR/$prog
			cat > $BINDIR/$prog <<EOF
#!/bin/sh
exec \`dirname \$0\`/../staging_dir/$toolchain_dir/bin/$cross_bin "\$@"
EOF
			chmod +x $BINDIR/$prog
		else
			ln -sf ../staging_dir/$toolchain_dir/bin/$cross_bin $BINDIR/$prog
		fi
	done
	[ -e $BINDIR/cc ] || ln -sf gcc $BINDIR/cc

	# Link files that exist in 'target-xxxx' but not exist in 'toolchain-xxxx' */
	if [ -d staging_dir/$target_dir/usr ]; then
		local f
		# library files
		for f in staging_dir/$target_dir/usr/lib/*; do
			[ -e $f ] || continue
			local f=`basename $f`
			[ -e staging_dir/$toolchain_dir/lib/$f ] && continue || :
			ln -sf ../../$target_dir/usr/lib/$f staging_dir/$toolchain_dir/lib/$f
			echo staging_dir/$toolchain_dir/lib/$f >> $LOGFILE
		done
		# header files
		for f in staging_dir/$target_dir/usr/include/*; do
			[ -e $f ] || continue
			local f=`basename $f`
			[ -e staging_dir/$toolchain_dir/include/$f ] && continue || :
			ln -sf ../../$target_dir/usr/include/$f staging_dir/$toolchain_dir/include/$f
			echo staging_dir/$toolchain_dir/include/$f >> $LOGFILE
		done
	fi

	# Remove duplicate items
	if [ -f $LOGFILE ]; then
		sort -u $LOGFILE > $LOGFILE-
		mv -f $LOGFILE- $LOGFILE || rm -f $LOGFILE-
	fi

	echo "Done!" >&2
)
}

do_start()
{
	local openwrt_root=`readlink -f "$1"`; shift 1
	local toolchain_dir=`get_toolchain_dir "$openwrt_root"`
	local target_dir=`get_target_dir "$openwrt_root"`

	if [ -z "$toolchain_dir" ]; then
		echo "*** Cannot get a possible 'staging_dir/toolchain-xxxx' directory. Please make sure you are under OpenWrt source root." >&2
		return 1
	fi
	if [ -z "$target_dir" ]; then
		echo "Warning: Directory 'staging_dir/target-xxxx' does not exist. Using a toolchain?" >&2
	fi

	eval `grep '^CONFIG_ARCH=' $openwrt_root/.config | head -n1`
	local arch_type="$CONFIG_ARCH"
	[ -n "$arch_type" ] || arch_type=`basename $openwrt_root`

	if [ "$SETUP_TOOLCHAIN" = Y -o ! -d $openwrt_root/$BINDIR ]; then
		setup_toolchain || return 1
	fi

	# Find the kernel source tree directory */
	local kernel_dir=
	local dir
	for dir in $openwrt_root/build_dir/linux-*/linux-* $openwrt_root/build_dir/target-*/linux-*/linux-*; do
		if [ -d $dir/include/linux ]; then
			kernel_dir=$dir
			break
		fi
	done
	if [ -n "$kernel_dir" ]; then
		export KERNSRC=$kernel_dir KERNELPATH=$kernel_dir
	fi

	# PATH for 'mips-openwrt-linux-gcc', ...
	PATH="$openwrt_root/staging_dir/$toolchain_dir/bin:$PATH"
	# PATH for fake 'gcc', ...
	if [ "$PATH_CHEATING" = Y ]; then
		PATH="$openwrt_root/$BINDIR:$PATH"
	fi
	export PATH
	export STAGING_DIR=$openwrt_root/staging_dir/$target_dir

	# Kernel module cross compile requires a valid 'ARCH' env
	case "$arch_type" in
		mips*) export ARCH=mips;;
	esac

	if [ $# -eq 0 ]; then
		exec bash --rcfile <( [ -f ~/.bashrc ] && cat ~/.bashrc; echo "PS1='[\[\e[1;33m\]cross@$arch_type:\[\e[m\]\w]\$ '" )
	else
		exec "$@"
	fi
}

do_clean()
{
	local openwrt_root=`readlink -f "$1"`

	[ -d $openwrt_root/$BINDIR ] || return 0
	cd $openwrt_root || return 1

	echo "Cleaning up the toolchain ..." >&2

	if [ -f $LOGFILE ]; then
		local f
		while read f; do
			rm -vf $f
		done < $LOGFILE
	fi
	rm -vrf $BINDIR

	echo "Done!" >&2
}

print_help()
{
	cat <<EOF
Script for "cheating" your shell environment by setting PATH with a directory
that contains "fake" GCC utilities (gcc, g++, ld, cc, ...) symbolic-linked to
OpenWrt cross compilers.
In this way developers can simply run 'gcc', 'g++' commands to cross-compile
C/C++ programs, or just run "make" to build simple Makefile-based C/C++ projects.

Usage:
 cross-compile.sh <openwrt_root>
         ... setup cross compiling with specified OpenWrt source tree,
             cross compilers can either be invoked by 'xxxx-openwrt-linux-gcc'
             or 'gcc'
 cross-compile.sh -n <openwrt_root>
         ... similar as above but do not cheat local compilers, but
             cross compilers can only be invoked by 'xxxx-openwrt-linux-gcc'
 cross-compile.sh -c <openwrt_root>
         ... remove all temporary symbolic links and directories

Instructions for different cross-compiling scenarios:
 1. "GNU configure" based projects:
    cross-compile.sh <openwrt_root> -n
    ./configure --host=xxxx-openwrt-linux
    make
 2. "GNU make" based projects:
    cross-compile.sh <openwrt_root>
    cd <project_dir>
    make

EOF
}


#
# Starts here
#
CLEANUP_MODE=N

while [ -n "$1" ]; do
	case "$1" in
		-n) PATH_CHEATING=N;;
		-N) SETUP_TOOLCHAIN=N;;
		-c) CLEANUP_MODE=Y;;
		-h) print_help;;
		*) break;;
	esac
	shift 1
done
if [ -z "$1" ]; then
	print_help
	exit 1
fi

openwrt_root="$1"; shift 1

case "$1" in
	-n) PATH_CHEATING=N; shift 1;;
	-N) SETUP_TOOLCHAIN=N; shift 1;;
	-c) CLEANUP_MODE=Y; shift 1;;
esac

if [ "$CLEANUP_MODE" = Y ]; then
	do_clean "$openwrt_root"
else
	do_start "$openwrt_root" "$@"
fi
