.PHONY:all clean
LDFLAGS := -m elf_i386
AFLAGS := -f elf
CFLAGS := -std=c++11 -m32 -c -Wall -I ./lib/inc  -I ./kernel/inc -I ./device/inc -I ./thread/inc \
-I ./userprocess/inc -fno-stack-protector 

#k_file := $(wildcard kernel/*.cpp)
#这里要保证调用者文件在前面，他是根据调用者调用的符号查找下一个文件并链接的
obj := build/main.o build/k_printf.o build/string.o build/itoa.o build/init.o build/interrupt.o \
build/kernel.o build/timer.o  build/list.o build/bitmap.o build/thread.o build/memory.o build/switch.o \
build/console.o build/sync.o build/keyboard.o build/process.o build/tss.o build/syscall.o \
build/syscall_init.o build/stdio.o

all:build/kernel.bin build/mbr.o build/loader.o 

#加@，则不会打印要执行的命令	
build/kernel.bin:$(obj)
	ld $(LDFLAGS) $^ -Ttext 0xc0001500 -e main -o $@  
	
build/%.o:kernel/src/%.cpp
	g++ $(CFLAGS) $< -o $@

build/%.o:lib/src/%.cpp
	g++ $(CFLAGS) $< -o $@

build/%.o:device/src/%.cpp
	g++ $(CFLAGS) $< -o $@

build/%.o:thread/src/%.cpp
	g++ $(CFLAGS) $< -o $@

build/%.o:userprocess/src/%.cpp
	g++ $(CFLAGS) $< -o $@

build/%.o:kernel/src/%.S
	nasm $(AFLAGS) $< -o $@

build/%.o:boot/%.S
	nasm -i boot/include/ $< -o $@

build/%.o:thread/%.S
	nasm $(AFLAGS) $< -o $@
dd:
	dd if=build/mbr.o of=c.img bs=512 count=1 conv=notrunc	
	dd if=build/loader.o of=c.img bs=512 seek=2 count=4 conv=notrunc	
	dd if=build/kernel.bin of=c.img bs=512 seek=9 count=200 conv=notrunc	
clean:
	-rm -rf build/*.o build/kernel.bin