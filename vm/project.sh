#!/bin/sh
# had to do this before running to get to work?
# export PATH="/home/justinb/git/buildroot/output/host/bin:${PATH}"
qemu-system-arm -M versatilepb -kernel zImage -dtb versatile-pb.dtb -drive file=rootfs.ext2,if=scsi,format=raw -append "root=/dev/sda console=ttyAMA0,115200" -serial stdio -net nic,model=rtl8139 -net user,hostfwd=tcp::2222-:22 -name Versatile_ARM_EXT2