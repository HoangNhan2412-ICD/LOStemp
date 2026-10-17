// Chặn việc khai báo trùng khi header được include nhiều lần.
#ifndef ASGN1_IOCTL_H
// Đánh dấu header đã được xử lý trong đơn vị biên dịch hiện tại.
#define ASGN1_IOCTL_H
// Cung cấp macro mã hóa hướng truyền, nhóm, số thứ tự và kích thước ioctl.
#include <linux/ioctl.h>

// Chọn nhóm ioctl A; kernel và chương trình test phải dùng cùng giá trị.
#define ASGN1_MAGIC 'A'
// Giới hạn vùng trả về phiên bản đúng 64 byte theo đề.
#define ASGN1_VERSION_SIZE 64
// Dung lượng buffer, gồm cả một byte kết thúc chuỗi.
#define ASGN1_CAPACITY 1024

// Định nghĩa ABI thống kê dùng chung giữa module và userspace.
struct asgn1_stats {
    // Tổng số lần mở thành công kể từ khi nạp module.
    int open_count;
    // Số lần ghi thành công kể từ lần RESET gần nhất.
    int write_count;
    // Số lần đọc trả byte dương kể từ lần RESET gần nhất.
    int read_count;
    // Số byte dữ liệu hiện có, gồm tiền tố nếu bật Kernel mode.
    int buffer_len;
    // Số byte userspace gửi trong lần ghi thành công gần nhất.
    int last_write_size;
};
// Command không có đối số; số thứ tự 0.
#define ASGN1_RESET_BUFFER _IO(ASGN1_MAGIC, 0)
// Command đọc struct thống kê từ kernel; size suy ra từ kiểu struct.
#define ASGN1_GET_STATS _IOR(ASGN1_MAGIC, 1, struct asgn1_stats)
// Command gửi một int mode từ userspace vào kernel.
#define ASGN1_SET_MODE _IOW(ASGN1_MAGIC, 2, int)
// Command nhận đúng vùng version 64 byte từ kernel.
#define ASGN1_GET_VERSION _IOR(ASGN1_MAGIC, 3, char[ASGN1_VERSION_SIZE])
// Kết thúc điều kiện bảo vệ header.
#endif
