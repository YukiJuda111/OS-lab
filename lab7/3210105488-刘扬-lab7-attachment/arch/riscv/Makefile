all:
	${MAKE} -C kernel all
	${LD} -T kernel/vmlinux.lds kernel/*.o ../../init/*.o ../../lib/*.o ../../user/ramdisk ../../fs/*.o -o ../../vmlinux ../../test/*.o
	$(shell test -d boot || mkdir -p boot)
	${OBJCOPY} -O binary ../../vmlinux ./boot/Image
	nm ../../vmlinux >  ../../System.map

clean:
	${MAKE} -C kernel clean
	$(shell test -d boot && rm -rf boot)
