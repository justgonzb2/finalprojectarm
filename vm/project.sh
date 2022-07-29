#!/bin/sh
exec qemu-system-arm -M versatilepb -kernel zImage -dtb versatile-pb.dtb -drive file=rootfs.ext2,if=scsi,format=raw -append "rootwait root=/dev/sda console=ttyAMA0,115200"  -net nic,model=rtl8139 -net user,hostfwd=tcp::2222-:22
