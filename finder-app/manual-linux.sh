#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-


if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

if [ ! -d "${OUTDIR}" ]; then
    mkdir -p ${OUTDIR}
    if [ $? -ne 0 ]; then
	echo "Failed to create ${OUTDIR}"
	exit 1
    fi
fi    


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # One time build system updates with required packages
    #sudo apt-get install -y --no-install-recommends bc u-boot-tools kmod \
    #	                    cpio flex bison libssl-dev libelf-dev psmisc && \
    #sudo apt-get install -y qemu-system-arm

    # NOTE: Assumes that the build toolchain for the CROSS_COMPILE arch
    # is in PATH; ARCH and CROSS_COMPILE already defined above
    # Clean out previous build
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    if [ $? -ne 0 ] ; then
	echo "Clean failed"
	exit 1
    fi
    # Set up default config
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig   
    if [ $? -ne 0 ] ; then
	echo "Build config failed"
	exit 1
    fi

    # Uncomment to modify default config
    # Backup the old config first
    # if [ -f ".config" ]; then
    #   cp .config ../bkp.build.config
    # fi
    # make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} menuconfig

    # Build
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    if [ $? -ne 0 ] ; then
      echo "Build all failed"
      exit 1
    fi
    
    # Skip modules - requires starting the emulator with >512MB memory
    # make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    # if [ $? -ne 0 ] ; then
    #	echo "Build modules failed"
    #	exit 1
    # fi

    # Device tree - should not make all build the dtbs target as well?
    # But no matter - the call is short-circuited if it is
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    if [ $? -ne 0 ] ; then
      echo "Build dtbs failed"
      exit 1
    fi

    echo "Adding the Image in outdir"
    cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/
fi



echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi
# TODO: Create necessary base directories
mkdir ${OUTDIR}/rootfs && cd ${OUTDIR}/rootfs
mkdir bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX="${OUTDIR}/rootfs" install

cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp ${FINDER_APP_DIR}/toolchain_lib_dep/ld-linux-aarch64.so.1 ./lib/
cp ${FINDER_APP_DIR}/toolchain_lib_dep/libm.so.6 ./lib64/
cp ${FINDER_APP_DIR}/toolchain_lib_dep/libresolv.so.2 ./lib64/
cp ${FINDER_APP_DIR}/toolchain_lib_dep/libc.so.6 ./lib64/

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
cd "$FINDER_APP_DIR"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} clean
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp finder.sh ${OUTDIR}/rootfs/home/
cp finder-test.sh ${OUTDIR}/rootfs/home/
cp autorun-qemu.sh ${OUTDIR}/rootfs/home/
cp writer ${OUTDIR}/rootfs/home/
if [ ! -d "${OUTDIR}/rootfs/home/conf" ];
then
    mkdir ${OUTDIR}/rootfs/home/conf
fi
cp conf/*.txt ${OUTDIR}/rootfs/home/conf/

# TODO: Chown the root directory
sudo chown root:root ${OUTDIR}/rootfs 

# TODO: Create initramfs.cpio.gz
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio

