#!/bin/sh
# Dừng khi lệnh lỗi hoặc dùng biến chưa được khai báo.
set -eu
# Chuyển đến thư mục chứa script để đường dẫn module/test hoạt động đúng.
cd "$(dirname "$0")"
# In từng lệnh được chạy để log thể hiện đầy đủ quá trình test.
set -x
# Nạp module vào kernel của guest QEMU; không chạy script này trên host.
insmod ./asgn1_driver.ko
# Kiểm tra module còn có trong /proc/modules, không chỉ dựa vào mã trả về rmmod.
grep '^asgn1_driver ' /proc/modules
# Tạo character node major 241 minor 0 nếu chưa có node thiết bị.
if [ ! -c /dev/asgn1 ]; then mknod /dev/asgn1 c 241 0; fi
# In kiểu node và major/minor làm minh chứng đăng ký thiết bị.
ls -l /dev/asgn1
# Chạy chương trình C kiểm tra chức năng, mã lỗi, giới hạn và đồng thời.
./asgn1_test
# Dùng cat để kiểm tra buffer rỗng và lệnh đọc kết thúc được.
test -z "$(cat /dev/asgn1)"
# Giữ descriptor 9 mở để kiểm tra reference count ngăn gỡ module.
exec 9<> /dev/asgn1
# Thử gỡ khi còn mở; BusyBox có thể báo lỗi nhưng trả 0, nên kiểm tra proc ở dòng sau.
rmmod asgn1_driver || true
# Kiểm tra module còn có trong /proc/modules, không chỉ dựa vào mã trả về rmmod.
grep '^asgn1_driver ' /proc/modules
# Đóng descriptor 9 để module có thể được gỡ bình thường.
exec 9>&-
# Gỡ module sau khi không còn descriptor của test giữ mở.
rmmod asgn1_driver
# Xác nhận tài nguyên procfs/sysfs/device node đã được dọn sau khi gỡ.
test ! -e /proc/asgn1_info
# Báo lỗi nếu module vẫn còn đăng ký sau thao tác gỡ bình thường.
if grep -q '^asgn1_driver ' /proc/modules; then exit 1; fi
# Lấy log thật của driver và kiểm tra thông báo cleanup.
dmesg | grep 'asgn1_driver:'
# Lấy log thật của driver và kiểm tra thông báo cleanup.
dmesg | grep 'asgn1_driver: cleanup complete'
# Nạp module vào kernel của guest QEMU; không chạy script này trên host.
insmod ./asgn1_driver.ko
# Dùng cat để kiểm tra buffer rỗng và lệnh đọc kết thúc được.
test -z "$(cat /dev/asgn1)"
# Ghi dữ liệu kiểm tra mode Echo mặc định sau khi reload.
printf 'default-echo' > /dev/asgn1
# Đọc device và đối chiếu chính xác chuỗi/số mong đợi.
test "$(cat /dev/asgn1)" = 'default-echo'
# Gỡ module sau khi không còn descriptor của test giữ mở.
rmmod asgn1_driver
# In marker thành công sau khi tất cả điều kiện trong script đã đạt.
echo TASK1_SCRIPT_PASS
