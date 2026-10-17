#!/bin/sh
# Dừng khi lệnh lỗi hoặc dùng biến chưa được khai báo.
set -eu
# Chuyển đến thư mục chứa script để đường dẫn module/test hoạt động đúng.
cd "$(dirname "$0")"
# In từng lệnh được chạy để log thể hiện đầy đủ quá trình test.
set -x
# Nạp module vào kernel của guest QEMU; không chạy script này trên host.
insmod ./counter_driver.ko
# Kiểm tra module còn có trong /proc/modules, không chỉ dựa vào mã trả về rmmod.
grep '^counter_driver ' /proc/modules
# Xác nhận misc framework đã tạo character device node.
test -c /dev/counter
# In kiểu node và major/minor làm minh chứng đăng ký thiết bị.
ls -l /dev/counter
# Gắn nhãn thời điểm để phân biệt các snapshot thống kê trong log.
echo 'SNAPSHOT BEFORE WRITE'
# In snapshot thống kê thực tế tại thời điểm đang kiểm tra.
cat /proc/counter_info
# Đọc device và đối chiếu chính xác chuỗi/số mong đợi.
test "$(cat /dev/counter)" = 0
# Đọc device và đối chiếu chính xác chuỗi/số mong đợi.
test "$(cat /dev/counter)" = 1
# Đặt counter thành 100 bằng giao diện write của device.
echo 100 > /dev/counter
# Gắn nhãn thời điểm để phân biệt các snapshot thống kê trong log.
echo 'SNAPSHOT AFTER WRITE'
# In snapshot thống kê thực tế tại thời điểm đang kiểm tra.
cat /proc/counter_info
# Đọc device và đối chiếu chính xác chuỗi/số mong đợi.
test "$(cat /dev/counter)" = 100
# Reset counter về 0 bằng giao diện write của device.
echo 0 > /dev/counter
# Gắn nhãn thời điểm để phân biệt các snapshot thống kê trong log.
echo 'SNAPSHOT AFTER RESET'
# In snapshot thống kê thực tế tại thời điểm đang kiểm tra.
cat /proc/counter_info
# Đọc device và đối chiếu chính xác chuỗi/số mong đợi.
test "$(cat /dev/counter)" = 0
# Chạy chương trình C kiểm tra chức năng, mã lỗi, giới hạn và đồng thời.
./counter_test
# Giữ descriptor 9 mở để kiểm tra reference count ngăn gỡ module.
exec 9<> /dev/counter
# Thử gỡ khi còn mở; BusyBox có thể báo lỗi nhưng trả 0, nên kiểm tra proc ở dòng sau.
rmmod counter_driver || true
# Kiểm tra module còn có trong /proc/modules, không chỉ dựa vào mã trả về rmmod.
grep '^counter_driver ' /proc/modules
# Đóng descriptor 9 để module có thể được gỡ bình thường.
exec 9>&-
# Gỡ module sau khi không còn descriptor của test giữ mở.
rmmod counter_driver
# Báo lỗi nếu module vẫn còn đăng ký sau thao tác gỡ bình thường.
if grep -q '^counter_driver ' /proc/modules; then exit 1; fi
# Xác nhận tài nguyên procfs/sysfs/device node đã được dọn sau khi gỡ.
test ! -e /proc/counter_info
# Xác nhận tài nguyên procfs/sysfs/device node đã được dọn sau khi gỡ.
test ! -e /sys/class/misc/counter
# Xác nhận tài nguyên procfs/sysfs/device node đã được dọn sau khi gỡ.
test ! -e /dev/counter
# Lấy log thật của driver và kiểm tra thông báo cleanup.
dmesg | grep 'counter_driver:'
# Lấy log thật của driver và kiểm tra thông báo cleanup.
dmesg | grep 'counter_driver: cleanup complete'
# Nạp module vào kernel của guest QEMU; không chạy script này trên host.
insmod ./counter_driver.ko
# Đọc device và đối chiếu chính xác chuỗi/số mong đợi.
test "$(cat /dev/counter)" = 0
# Gỡ module sau khi không còn descriptor của test giữ mở.
rmmod counter_driver
# In marker thành công sau khi tất cả điều kiện trong script đã đạt.
echo TASK2_SCRIPT_PASS
