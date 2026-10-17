// SPDX-License-Identifier: GPL-2.0

// Khai báo API vòng đời module và thông tin license, tác giả, phiên bản.
#include <linux/module.h>
// Khai báo inode, file, file_operations và API đăng ký character device.
#include <linux/fs.h>
// Cung cấp mutex để bảo vệ dữ liệu dùng chung giữa nhiều tiến trình.
#include <linux/mutex.h>
// Cung cấp phép sao chép an toàn giữa vùng nhớ kernel và userspace.
#include <linux/uaccess.h>
// Cung cấp API tạo và gỡ file thống kê trong /proc.
#include <linux/proc_fs.h>
// Hỗ trợ xuất dữ liệu procfs theo cơ chế seq_file.
#include <linux/seq_file.h>
// Cung cấp cấp phát và giải phóng bộ nhớ động trong kernel.
#include <linux/slab.h>
// Dùng chung định nghĩa command và cấu trúc dữ liệu ioctl với ứng dụng.
#include "asgn1_ioctl.h"

// Chọn major 241, nằm trong khoảng 241–254 mà đề yêu cầu.
#define ASGN1_MAJOR 241
// Tạo mutex chung bảo vệ buffer, thống kê và chế độ ghi.
static DEFINE_MUTEX(state_lock);
// Tạo buffer dùng chung, được khởi tạo bằng 0 khi module nạp.
static char buffer[ASGN1_CAPACITY];
// Lưu năm trường thống kê; ban đầu tất cả bằng 0.
static struct asgn1_stats stats;
// Mặc định Echo mode: lưu nguyên dữ liệu người dùng ghi.
static int mode = 1;
// Giữ handle procfs để gỡ đúng tài nguyên khi thoát module.
static struct proc_dir_entry *info;

// Mở thiết bị: inode xác định minor, file giữ trạng thái VFS; trả 0 hoặc mã lỗi.
static int asgn1_open(struct inode *inode, struct file *file)
{
    // Chỉ hỗ trợ minor 0 mặc dù register_chrdev dành nhiều minor.
    if (iminor(inode) != 0)
        // Từ chối truy cập device minor không được driver hỗ trợ.
        return -ENODEV;
    // Khóa trạng thái chung trước khi đọc hoặc sửa dữ liệu.
    mutex_lock(&state_lock);
    // Giới hạn bộ đếm mở để không tràn số nguyên có dấu.
    if (stats.open_count < INT_MAX)
        // Tăng tổng số lần mở thiết bị thành công.
        stats.open_count++;
    // Nhả khóa trạng thái chung để tiến trình khác tiếp tục.
    mutex_unlock(&state_lock);
    // Đánh dấu thiết bị không hỗ trợ seek và trả trạng thái mở cho VFS.
    return nonseekable_open(inode, file);
}

// Đọc buffer: file là ngữ cảnh, dst là đích user; trả số byte, 0 khi EOF hoặc lỗi âm.
static ssize_t asgn1_read(struct file *file, char __user *dst,
                          // count là số byte yêu cầu; pos trỏ tới offset của file do VFS quản lý.
                          size_t count, loff_t *pos)
{
    // Lưu số byte xử lý thành công hoặc mã lỗi âm.
    ssize_t ret;
    // Khóa trạng thái chung trước khi đọc hoặc sửa dữ liệu.
    mutex_lock(&state_lock);
    // Chép tối đa count byte từ offset hiện tại; helper cập nhật offset và trả 0 ở EOF.
    ret = simple_read_from_buffer(dst, count, pos, buffer, stats.buffer_len);
    // Chỉ đếm lần đọc thực sự trả dữ liệu; không đếm EOF hoặc lỗi.
    if (ret > 0 && stats.read_count < INT_MAX)
        // Tăng bộ đếm đọc thành công có dữ liệu.
        stats.read_count++;
    // Nhả khóa trạng thái chung để tiến trình khác tiếp tục.
    mutex_unlock(&state_lock);
    // Trả kết quả thao tác cho bên gọi.
    return ret;
}

// Ghi thay nội dung: src là dữ liệu user, file là ngữ cảnh; trả số byte hoặc lỗi, không dùng offset ghi.
static ssize_t asgn1_write(struct file *file, const char __user *src,
                           // count là số byte yêu cầu; pos trỏ tới offset của file do VFS quản lý.
                           size_t count, loff_t *pos)
{
    // Con trỏ tới bản sao tạm dữ liệu userspace trong heap kernel.
    char *input;
    // Số byte dành cho tiền tố, bằng 8 ở Kernel mode và 0 ở Echo mode.
    size_t prefix;
    // Xử lý yêu cầu có độ dài 0 mà không thay đổi dữ liệu.
    if (!count)
        // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
        return 0;
    // Chừa một byte NUL; không nhận dữ liệu chiếm toàn bộ buffer.
    if (count >= ASGN1_CAPACITY)
        // Báo dữ liệu quá dài, giữ nguyên nội dung buffer trước đó.
        return -EMSGSIZE;
    // Cấp phát và sao chép dữ liệu user vào buffer tạm; tránh buffer lớn trên stack.
    input = memdup_user(src, count);
    // Kiểm tra lỗi cấp phát hoặc lỗi truy cập userspace từ memdup_user.
    if (IS_ERR(input))
        // Lấy mã lỗi được mã hóa trong con trỏ và trả lại bên gọi.
        return PTR_ERR(input);
    // Khóa trạng thái chung trước khi đọc hoặc sửa dữ liệu.
    mutex_lock(&state_lock);
    // Tính độ dài tiền tố theo mode; không tính byte NUL của chuỗi hằng.
    prefix = mode == 0 ? sizeof("[KERNEL]") - 1 : 0;
    // Kiểm tra tổng tiền tố và dữ liệu vẫn còn chỗ cho NUL.
    if (count + prefix >= sizeof(buffer)) {
        // Nhả khóa trạng thái chung để tiến trình khác tiếp tục.
        mutex_unlock(&state_lock);
        // Giải phóng bản sao tạm, tránh rò rỉ bộ nhớ.
        kfree(input);
        // Báo dữ liệu quá dài, giữ nguyên nội dung buffer trước đó.
        return -EMSGSIZE;
    }
    // Chỉ thêm tiền tố khi đang ở Kernel mode.
    if (prefix)
        // Chép tiền tố giữa hai vùng kernel, không truy cập userspace bằng memcpy.
        memcpy(buffer, "[KERNEL]", prefix);
    // Chép dữ liệu tạm sau tiền tố; cả nguồn và đích đều thuộc kernel.
    memcpy(buffer + prefix, input, count);
    // Thêm NUL sau phần dữ liệu hợp lệ.
    buffer[prefix + count] = '\0';
    // Ghi độ dài dữ liệu đã lưu, bao gồm tiền tố nếu có.
    stats.buffer_len = prefix + count;
    // Ghi số byte người dùng gửi, không tính tiền tố do driver thêm.
    stats.last_write_size = count;
    // Ngăn bộ đếm ghi tràn số nguyên có dấu.
    if (stats.write_count < INT_MAX)
        // Tăng số lần ghi thành công.
        stats.write_count++;
    // Nhả khóa trạng thái chung để tiến trình khác tiếp tục.
    mutex_unlock(&state_lock);
    // Giải phóng bản sao tạm, tránh rò rỉ bộ nhớ.
    kfree(input);
    // Báo đã nhận đúng số byte người dùng yêu cầu.
    return count;
}

// Điều khiển thiết bị: cmd chọn thao tác, arg chứa con trỏ user nếu cần; trả 0 hoặc errno âm.
static long asgn1_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    // Diễn giải đối số ioctl thành con trỏ userspace; chưa dereference trực tiếp.
    void __user *p = (void __user *)arg;
    // Bản chụp thống kê dùng để trả về ngoài vùng khóa.
    struct asgn1_stats snapshot;
    // Lưu mode lấy từ userspace sau khi copy thành công.
    int requested;
    // Khởi tạo chuỗi version và điền 0 phần còn lại, tránh lộ dữ liệu stack.
    const char version[ASGN1_VERSION_SIZE] = "asgn1_driver v1.0 - SE200778";
    // Ghi command và số thứ tự vào dmesg mỗi lần gọi ioctl.
    pr_info("asgn1_driver: ioctl cmd=0x%x nr=%u\n", cmd, _IOC_NR(cmd));
    // So khớp toàn bộ command, gồm hướng, nhóm, số thứ tự và kích thước.
    switch (cmd) {
    // Nhánh reset buffer và bộ đếm đọc/ghi.
    case ASGN1_RESET_BUFFER:
        // Khóa trạng thái chung trước khi đọc hoặc sửa dữ liệu.
        mutex_lock(&state_lock);
        // Xóa toàn bộ buffer bằng byte 0.
        memset(buffer, 0, sizeof(buffer));
        // Đặt độ dài và hai bộ đếm về 0; giữ tổng open_count.
        stats.buffer_len = stats.write_count = stats.read_count = 0;
        // Xóa kích thước lần ghi cuối sau reset.
        stats.last_write_size = 0;
        // Nhả khóa trạng thái chung để tiến trình khác tiếp tục.
        mutex_unlock(&state_lock);
        // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
        return 0;
    // Nhánh lấy đủ năm trường thống kê.
    case ASGN1_GET_STATS:
        // Khóa trạng thái chung trước khi đọc hoặc sửa dữ liệu.
        mutex_lock(&state_lock);
        // Chụp thống kê trong khi mutex vẫn được giữ.
        snapshot = stats;
        // Nhả khóa trạng thái chung để tiến trình khác tiếp tục.
        mutex_unlock(&state_lock);
        // Chép thống kê an toàn ra user; có byte chưa chép thì trả EFAULT.
        return copy_to_user(p, &snapshot, sizeof(snapshot)) ? -EFAULT : 0;
    // Nhánh đặt chế độ hoạt động từ một int userspace.
    case ASGN1_SET_MODE:
        // Đọc mode an toàn; kết quả khác 0 nghĩa là chưa chép đủ dữ liệu.
        if (copy_from_user(&requested, p, sizeof(requested)))
            // Báo địa chỉ userspace không truy cập được.
            return -EFAULT;
        // Chấp nhận đúng hai mode mà giao diện quy định.
        if (requested != 0 && requested != 1)
            // Báo đối số hoặc nội dung dữ liệu không hợp lệ.
            return -EINVAL;
        // Khóa trạng thái chung trước khi đọc hoặc sửa dữ liệu.
        mutex_lock(&state_lock);
        // Cập nhật mode khi đã giữ khóa trạng thái.
        mode = requested;
        // Nhả khóa trạng thái chung để tiến trình khác tiếp tục.
        mutex_unlock(&state_lock);
        // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
        return 0;
    // Nhánh trả về chuỗi version trong vùng 64 byte.
    case ASGN1_GET_VERSION:
        // Chép tối đa đúng 64 byte; đổi lỗi truy cập thành EFAULT.
        return copy_to_user(p, version, sizeof(version)) ? -EFAULT : 0;
    // Xử lý những command không khớp định nghĩa hỗ trợ.
    default:
        // Báo ioctl không được hỗ trợ, đúng yêu cầu của đề.
        return -ENOTTY;
    }
}

// Xuất thống kê vào seq_file m; v không dùng; trả 0 khi ghi thông tin xong.
static int asgn1_status_show(struct seq_file *m, void *v)
{
    // Khóa trạng thái chung trước khi đọc hoặc sửa dữ liệu.
    mutex_lock(&state_lock);
    // Ghi các trường có nhãn vào seq_file để người dùng đọc qua /proc.
    seq_printf(m, "open_count: %d\nwrite_count: %d\nread_count: %d\nbuffer_len: %d\nlast_write_size: %d\nmode: %d\n",
               // Truyền các giá trị thống kê vào chuỗi định dạng, khi mutex vẫn đang giữ.
               stats.open_count, stats.write_count, stats.read_count,
               // Truyền các giá trị thống kê vào chuỗi định dạng, khi mutex vẫn đang giữ.
               stats.buffer_len, stats.last_write_size, mode);
    // Nhả khóa trạng thái chung để tiến trình khác tiếp tục.
    mutex_unlock(&state_lock);
    // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
    return 0;
}
// Sinh open và proc_ops cho seq_file; dùng tên riêng để tránh trùng callback device.
DEFINE_PROC_SHOW_ATTRIBUTE(asgn1_status);

// Khai báo các callback VFS của /dev/asgn1.
static const struct file_operations asgn1_fops = {
    // Giữ tham chiếu module khi file đang được sử dụng.
    .owner = THIS_MODULE,
    // Gọi asgn1_open khi mở thiết bị.
    .open = asgn1_open,
    // Gọi asgn1_read khi ứng dụng đọc.
    .read = asgn1_read,
    // Gọi asgn1_write khi ứng dụng ghi.
    .write = asgn1_write,
    // Nối syscall ioctl tới hàm điều khiển của driver.
    .unlocked_ioctl = asgn1_ioctl,
    // Không cho phép thay đổi offset bằng lseek.
    .llseek = no_llseek,
};

// Hàm nạp Task 1, không nhận tham số; đăng ký tài nguyên và rollback nếu lỗi; trả 0/errno.
static int __init asgn1_init(void)
{
    // Đăng ký major 241 với bảng callback của Task 1.
    int ret = register_chrdev(ASGN1_MAJOR, "asgn1", &asgn1_fops);
    // Kiểm tra lỗi đăng ký được biểu diễn bằng mã âm.
    if (ret < 0)
        // Trả kết quả thao tác cho bên gọi.
        return ret;
    // Tạo /proc/asgn1_info chỉ đọc với các callback thống kê.
    info = proc_create("asgn1_info", 0444, NULL, &asgn1_status_proc_ops);
    // Nếu tạo procfs thất bại, thực hiện rollback tài nguyên đã đăng ký.
    if (!info) {
        // Trả lại major đã đăng ký và gỡ character device khỏi kernel.
        unregister_chrdev(ASGN1_MAJOR, "asgn1");
        // Báo không đủ bộ nhớ hoặc không tạo được tài nguyên cần thiết.
        return -ENOMEM;
    }
    // Ghi sự kiện nạp/gỡ hoặc trạng thái driver vào dmesg để kiểm chứng.
    pr_info("asgn1_driver: loaded major=%d mode=1\n", ASGN1_MAJOR);
    // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
    return 0;
}

// Hàm gỡ Task 1; không có tham số/giá trị trả về; giải phóng tài nguyên đã đăng ký.
static void __exit asgn1_exit(void)
{
    // Gỡ file procfs và chờ các thao tác liên quan kết thúc.
    proc_remove(info);
    // Trả lại major đã đăng ký và gỡ character device khỏi kernel.
    unregister_chrdev(ASGN1_MAJOR, "asgn1");
    // Ghi sự kiện nạp/gỡ hoặc trạng thái driver vào dmesg để kiểm chứng.
    pr_info("asgn1_driver: cleanup complete\n");
}
// Đăng ký hàm được kernel gọi khi insmod nạp module.
module_init(asgn1_init);
// Đăng ký hàm dọn tài nguyên được gọi khi rmmod gỡ module.
module_exit(asgn1_exit);
// Khai báo giấy phép GPL cho module.
MODULE_LICENSE("GPL");
// Ghi họ tên và MSSV được cung cấp, hiển thị bằng modinfo.
MODULE_AUTHOR("Phạm Võ Hoàng Nhân - SE200778");
// Mô tả chức năng module để nhận diện trong modinfo.
MODULE_DESCRIPTION("ASM1 mutex-protected buffer and ioctl interface");
// Khai báo phiên bản triển khai 1.0.
MODULE_VERSION("1.0");
