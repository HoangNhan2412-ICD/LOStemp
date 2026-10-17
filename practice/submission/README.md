# ASM1 — Phạm Võ Hoàng Nhân — SE200778

Đề: `ASSIGNMENT01_EmbeddedLinux.docx` trong repository LOStemp, mục III–V.
Mã nguồn có chú thích tiếng Việt giải thích các thao tác, tham số và kết quả.
Bộ tài liệu đi kèm log kiểm thử thực tế trên kernel dyk trong QEMU.

## Nội dung

- `task1_ioctl/`: driver major 241, minor 0; header ioctl; executable test ARM tĩnh và shell test.
- `task2_misc/`: misc counter major 10, minor động; executable test ARM tĩnh và shell test.
- `logs/`: kết quả chạy thật; log lỗi ban đầu được giữ lại, không thay bằng kết quả giả.
- `bao_cao/SE200778_Asgn01_BaoCao.pdf`: báo cáo 15 trang, nội dung 12pt.
- `bao_cao/BaoCao.md`: nội dung văn bản; sơ đồ nằm trong PDF và script tạo báo cáo.

## Build trên host AlmaLinux

Trong checkout đang dùng, mặc định Makefile trỏ tới `practice/environment/build-dyk`.
Chỉ chạy từ bên trong practice/, không build tại thư mục file mẫu gốc:

```bash
make -C practice/submission/task1_ioctl
make -C practice/submission/task2_misc
python3 practice/tools/run_qemu.py
```

Nếu giải nén gói sang vị trí khác trong practice/, truyền đường dẫn tuyệt đối
đến kernel build đã chuẩn bị:

```bash
make KDIR=/home/nhan/Documents/Losproject/practice/environment/build-dyk
make clean KDIR=/home/nhan/Documents/Losproject/practice/environment/build-dyk
```

Chạy hai lệnh trên ở từng thư mục task. `clean` chỉ xóa sản phẩm build của task;
đã được thử trên bản sao riêng để không mất kết quả đã kiểm thử.
Compiler: `arm-none-linux-gnueabihf-gcc` 13.2.1; ARCH=arm.
Không dùng module này trên kernel host x86_64.

## Kiểm thử trong guest QEMU

Host runner tạo initramfs mới bằng overlay lên bản sao rootfs của Lab 01,
khởi động nguyên kernel/DTB dyk trong `practice/environment/boot`, nạp module,
chạy test rồi tắt guest. Không gắn thư mục host vào guest; chỉ serial log đi ra host.
Mỗi lần chạy có thư mục riêng trong `practice/output/runs/`, giữ cả lần lỗi.

Nếu chạy tương tác trong guest có các file tương ứng:

```sh
sh /asm1/task1_ioctl/test_asgn1.sh
sh /asm1/task2_misc/test_counter.sh
```

Script giả định module chưa được load; không cưỡng bức gỡ module đã có.
Marker thành công: TASK1_SCRIPT_PASS, TASK2_SCRIPT_PASS, mã test 0.
BusyBox được cung cấp có thể in lỗi rmmod nhưng trả 0; script kiểm tra trạng thái
thật trong /proc/modules và procfs/sysfs sau mỗi bước thay vì chỉ tin exit status.

## Các quyết định thiết kế

- Stats Task 1: 5 int; buffer_len theo đặc tả 3.2, tương ứng buf_len trong rubric.
- open_count tích lũy số open; read_count chỉ đếm read trả byte dương.
- RESET giữ open_count/mode; xóa buffer, read/write count, last_write_size.
- Buffer tối đa 1023 byte, Kernel mode nhận tối đa 1015 byte; quá dài trả EMSGSIZE.
- Counter: mỗi open trả một snapshot có newline, tăng đúng một lần khi read thành công.
  Read ngắn không tăng lại, EOF trả 0. Ghi không tua lại snapshot của fd đã đọc.
- write_count tính tất cả lần set hợp lệ, kể cả reset về 0.
- Counter tại INT_MAX trả EOVERFLOW khi đọc để tránh wrap số âm.
- atomic_t cộng mutex bảo vệ giao dịch nhiều bước; mutex riêng bảo vệ context mỗi fd.
- Không dùng sysfs attributes riêng cho Task 1; Task 2 có sysfs do misc framework tạo.

## Giới hạn bằng chứng

Kernel dyk build bằng GCC 15.2; module bằng GCC 13.2. Không coi UTS_RELEASE/vermagic
là bằng chứng đủ: đã nạp, chạy test và gỡ thật trên kernel dyk. Không dùng force.
Symbol table được build cục bộ từ Linux 5.15/cấu hình tương ứng, không phải file của dyk.
Không chứng minh mọi ABI hoặc mọi tải; chưa chạy KASAN/kmemleak/fault injection.
Rà soát giải phóng tài nguyên và reload thành công không thay thế chứng minh không rò rỉ tuyệt đối.

## Tên gói và nộp bài

Gói cuối: `SE200778_PhamVoHoangNhan_Asgn01.tar.gz`.
PDF ngoài gói: `SE200778_PhamVoHoangNhan_Asgn01_BaoCao.pdf` (mục 4.4).
PDF bên trong: `bao_cao/SE200778_Asgn01_BaoCao.pdf` (mục 4.1).
Đề chưa có deadline cụ thể; không tự nộp LMS, commit hoặc push.
