# Phân biệt lúc kernel Kbuild đọc file với lúc người dùng chạy make.
ifneq ($(KERNELRELEASE),)
# Khai báo tên module rời cần tạo thành file .ko.
obj-m := counter_driver.o
# Nhánh lệnh tiện ích dành cho người dùng trên host.
else
# Đường dẫn mặc định tới kernel build; cho phép ghi đè bằng KDIR=...
KDIR ?= $(abspath ../../environment/build-dyk)
# Chọn kiến trúc ARM, không dùng kiến trúc x86_64 của host.
ARCH := arm
# Tiền tố bộ công cụ ARM; cho phép truyền giá trị khác trên dòng lệnh.
CROSS_COMPILE ?= arm-none-linux-gnueabihf-
# Đánh dấu target là tác vụ, không phải tên file đầu ra.
.PHONY: all modules clean
# Target mặc định: build module và executable kiểm thử userspace.
all: modules counter_test
# Target gọi hệ thống build kernel để biên dịch module.
modules:
# Gọi Kbuild với thư mục module M, kiến trúc và compiler đã chọn.
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
# Khai báo các source/header làm điều kiện để biên dịch lại chương trình test.
counter_test: counter_test.c
# Build executable ARM liên kết tĩnh; bật cảnh báo và coi cảnh báo là lỗi.
	$(CROSS_COMPILE)gcc -static -O2 -Wall -Wextra -Werror -o $@ $<
# Target dọn các file sinh ra từ quá trình build của task.
clean:
# Yêu cầu Kbuild dọn sản phẩm trong thư mục module hiện tại.
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
# Xóa executable test được sinh ra, không xóa source.
	$(RM) counter_test
# Kết thúc phân nhánh giữa Kbuild và make trên host.
endif
