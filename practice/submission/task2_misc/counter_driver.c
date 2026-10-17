// SPDX-License-Identifier: GPL-2.0
// Khai báo API vòng đời module và thông tin license, tác giả, phiên bản.
#include <linux/module.h>
// Khai báo framework misc device dùng chung major 10.
#include <linux/miscdevice.h>
// Khai báo inode, file, file_operations và API đăng ký character device.
#include <linux/fs.h>
// Cung cấp atomic_t và các thao tác nguyên tử trên bộ đếm.
#include <linux/atomic.h>
// Cung cấp mutex để bảo vệ dữ liệu dùng chung giữa nhiều tiến trình.
#include <linux/mutex.h>
// Cung cấp cấp phát và giải phóng bộ nhớ động trong kernel.
#include <linux/slab.h>
// Cung cấp phép sao chép an toàn giữa vùng nhớ kernel và userspace.
#include <linux/uaccess.h>
// Cung cấp API tạo và gỡ file thống kê trong /proc.
#include <linux/proc_fs.h>
// Hỗ trợ xuất dữ liệu procfs theo cơ chế seq_file.
#include <linux/seq_file.h>

// Khởi tạo giá trị counter nguyên tử bằng 0.
static atomic_t counter = ATOMIC_INIT(0);
// Khởi tạo tổng số snapshot đọc thành công bằng 0.
static atomic_t reads = ATOMIC_INIT(0);
// Khởi tạo tổng số lần set/reset hợp lệ bằng 0.
static atomic_t writes = ATOMIC_INIT(0);

// Khóa chung bảo vệ giao dịch snapshot + tăng và thao tác set.
static DEFINE_MUTEX(counter_lock);
// Giữ handle procfs để gỡ đúng tài nguyên khi thoát module.
static struct proc_dir_entry *info;
// Định nghĩa dữ liệu riêng cho từng lần mở counter.
struct read_context {
    // Khóa riêng tránh hai luồng dùng cùng file context cập nhật đồng thời.
    struct mutex lock;
    // Đánh dấu snapshot đã được trả ít nhất một byte thành công.
    bool ready;
    // Chứa số nguyên thập phân, newline và NUL.
    char text[16];
    // Độ dài snapshot hợp lệ, không tính NUL.
    size_t len;
};

// Mở counter: cấp context cho file, inode là ngữ cảnh VFS; trả 0 hoặc lỗi cấp phát.
static int counter_open(struct inode *inode, struct file *file)
{
    // Cấp phát context và khởi tạo toàn bộ bằng 0; có thể ngủ khi chờ bộ nhớ.
    struct read_context *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    // Kiểm tra cấp phát context thất bại.
    if (!ctx)
        // Báo không đủ bộ nhớ hoặc không tạo được tài nguyên cần thiết.
        return -ENOMEM;
    // Khởi tạo mutex của context vừa cấp phát.
    mutex_init(&ctx->lock);
    // Gắn context vào file để các lần read và release dùng lại.
    file->private_data = ctx;
    // Đánh dấu thiết bị không hỗ trợ seek và trả trạng thái mở cho VFS.
    return nonseekable_open(inode, file);
}

// Đóng file: giải phóng context thuộc file; inode không dùng; trả 0.
static int counter_release(struct inode *inode, struct file *file)
{
    // Giải phóng context khi tham chiếu cuối cùng tới file được đóng.
    kfree(file->private_data);
    // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
    return 0;
}

// Đọc snapshot của file vào dst; tăng một lần khi trả dữ liệu lần đầu; trả byte/EOF/lỗi.
static ssize_t counter_read(struct file *file, char __user *dst,
                            // count là số byte yêu cầu; pos trỏ tới offset của file do VFS quản lý.
                            size_t count, loff_t *pos)
{
    // Lấy context của lần mở file hiện tại.
    struct read_context *ctx = file->private_data;
    // Lưu số byte xử lý thành công hoặc mã lỗi âm.
    ssize_t ret;
    // Xử lý yêu cầu có độ dài 0 mà không thay đổi dữ liệu.
    if (!count)
        // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
        return 0;
    // Khóa context trước khi đọc/cập nhật snapshot và offset.
    mutex_lock(&ctx->lock);
    // Chỉ lấy snapshot mới nếu lần mở này chưa trả dữ liệu thành công.
    if (!ctx->ready) {
        // Khóa trạng thái toàn cục để snapshot và increment là một giao dịch.
        mutex_lock(&counter_lock);
        // Chặn tăng từ INT_MAX để counter không vòng sang số âm.
        if (atomic_read(&counter) == INT_MAX) {
            // Nhả khóa trạng thái counter toàn cục.
            mutex_unlock(&counter_lock);
            // Nhả khóa context của file hiện tại.
            mutex_unlock(&ctx->lock);
            // Báo không thể tăng tiếp mà vẫn giữ giá trị trong miền int không âm.
            return -EOVERFLOW;
        }
        // Định dạng giá trị hiện tại thành số có newline và lưu độ dài thực.
        ctx->len = scnprintf(ctx->text, sizeof(ctx->text), "%d\n", atomic_read(&counter));
        // Trả phần snapshot theo offset; hết dữ liệu thì trả EOF.
        ret = simple_read_from_buffer(dst, count, pos, ctx->text, ctx->len);
        // Chỉ cam kết snapshot và tăng counter nếu đã trả ít nhất một byte.
        if (ret > 0) {
            // Giữ snapshot này cho các lần đọc phần còn lại trên cùng file.
            ctx->ready = true;
            // Tăng counter một đơn vị bằng thao tác nguyên tử.
            atomic_inc(&counter);
            // Không để tổng lượt đọc bị tràn số nguyên.
            if (atomic_read(&reads) < INT_MAX)
                // Tăng số snapshot đọc thành công.
                atomic_inc(&reads);
            // Ghi snapshot đã đọc vào dmesg để đối chiếu test.
            pr_info("counter_driver: read snapshot=%s", ctx->text);
        }
        // Nhả khóa trạng thái counter toàn cục.
        mutex_unlock(&counter_lock);
    // Dùng nhánh còn lại khi điều kiện if không thỏa.
    } else {
        // Trả phần snapshot theo offset; hết dữ liệu thì trả EOF.
        ret = simple_read_from_buffer(dst, count, pos, ctx->text, ctx->len);
    }
    // Nhả khóa context của file hiện tại.
    mutex_unlock(&ctx->lock);
    // Trả kết quả thao tác cho bên gọi.
    return ret;
}

// Parse chuỗi src để đặt counter; file/offset không dùng để chọn vị trí ghi; trả byte hoặc lỗi.
static ssize_t counter_write(struct file *file, const char __user *src,
                             // count là số byte yêu cầu; pos trỏ tới offset của file do VFS quản lý.
                             size_t count, loff_t *pos)
{
    // Buffer nhỏ nhận chuỗi số từ userspace trên stack.
    char text[32];
    // Lưu số nguyên sau khi parse chuỗi.
    int value;
    // Xử lý yêu cầu có độ dài 0 mà không thay đổi dữ liệu.
    if (!count)
        // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
        return 0;
    // Chừa chỗ cho NUL và từ chối chuỗi quá dài.
    if (count >= sizeof(text))
        // Báo đối số hoặc nội dung dữ liệu không hợp lệ.
        return -EINVAL;
    // Chép chuỗi user vào kernel và kiểm tra số byte chưa chép.
    if (copy_from_user(text, src, count))
        // Báo địa chỉ userspace không truy cập được.
        return -EFAULT;
    // Từ chối NUL nhúng để không bỏ qua dữ liệu nằm sau điểm kết thúc giả.
    if (memchr(text, '\0', count))
        // Báo đối số hoặc nội dung dữ liệu không hợp lệ.
        return -EINVAL;
    // Kết thúc chuỗi trước khi đưa vào kstrtoint.
    text[count] = '\0';
    // Parse cơ số 10; từ chối lỗi cú pháp, vượt miền int hoặc giá trị âm.
    if (kstrtoint(text, 10, &value) || value < 0)
        // Báo đối số hoặc nội dung dữ liệu không hợp lệ.
        return -EINVAL;
    // Khóa trạng thái toàn cục để snapshot và increment là một giao dịch.
    mutex_lock(&counter_lock);
    // Đặt counter thành giá trị mới bằng thao tác nguyên tử.
    atomic_set(&counter, value);
    // Chặn tràn bộ đếm số lần set/reset.
    if (atomic_read(&writes) < INT_MAX)
        // Tăng số lần ghi hợp lệ, bao gồm cả set khác 0.
        atomic_inc(&writes);
    // Nhả khóa trạng thái counter toàn cục.
    mutex_unlock(&counter_lock);
    // Ghi giá trị vừa đặt vào log kernel.
    pr_info("counter_driver: set=%d\n", value);
    // Báo đã nhận đúng số byte người dùng yêu cầu.
    return count;
}

// Xuất snapshot counter và thống kê vào m; v không dùng; trả 0.
static int counter_status_show(struct seq_file *m, void *v)
{
    // Khóa trạng thái toàn cục để snapshot và increment là một giao dịch.
    mutex_lock(&counter_lock);
    // Ghi các trường có nhãn vào seq_file để người dùng đọc qua /proc.
    seq_printf(m, "counter: %d\nread_count: %d\nwrite_count: %d\n",
               // Truyền các giá trị thống kê vào chuỗi định dạng, khi mutex vẫn đang giữ.
               atomic_read(&counter), atomic_read(&reads), atomic_read(&writes));
    // Nhả khóa trạng thái counter toàn cục.
    mutex_unlock(&counter_lock);
    // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
    return 0;
}
// Sinh open và proc_ops đọc thống kê counter bằng seq_file.
DEFINE_PROC_SHOW_ATTRIBUTE(counter_status);
// Khai báo các callback VFS của /dev/counter.
static const struct file_operations counter_fops = {
    // Giữ module khi file mở; cấp phát context ở open và giải phóng ở release.
    .owner = THIS_MODULE, .open = counter_open, .release = counter_release,
    // Gắn callback đọc/ghi và không hỗ trợ seek.
    .read = counter_read, .write = counter_write, .llseek = no_llseek,
};
// Mô tả misc device cần đăng ký với kernel.
static struct miscdevice counter_device = {
    // Xin minor động, đặt tên counter và nối bảng callback.
    .minor = MISC_DYNAMIC_MINOR, .name = "counter", .fops = &counter_fops,
    // Đặt quyền device node: chỉ chủ sở hữu được đọc và ghi.
    .mode = 0600,
};

// Hàm nạp Task 2, không nhận tham số; đăng ký misc và procfs; trả 0/errno.
static int __init counter_init(void)
{
    // Đăng ký misc device và nhận minor động từ kernel.
    int ret = misc_register(&counter_device);
    // Kiểm tra API trả về mã lỗi khác 0.
    if (ret)
        // Trả kết quả thao tác cho bên gọi.
        return ret;
    // Tạo /proc/counter_info chỉ đọc với các callback thống kê.
    info = proc_create("counter_info", 0444, NULL, &counter_status_proc_ops);
    // Nếu tạo procfs thất bại, thực hiện rollback tài nguyên đã đăng ký.
    if (!info) {
        // Hủy đăng ký misc device và dọn tài nguyên framework quản lý.
        misc_deregister(&counter_device);
        // Báo không đủ bộ nhớ hoặc không tạo được tài nguyên cần thiết.
        return -ENOMEM;
    }
    // Ghi sự kiện nạp/gỡ hoặc trạng thái driver vào dmesg để kiểm chứng.
    pr_info("counter_driver: loaded major=10 minor=%d\n", counter_device.minor);
    // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
    return 0;
}

// Hàm gỡ Task 2; không có tham số/giá trị trả về; gỡ procfs và misc device.
static void __exit counter_exit(void)
{
    // Gỡ file procfs và chờ các thao tác liên quan kết thúc.
    proc_remove(info);
    // Hủy đăng ký misc device và dọn tài nguyên framework quản lý.
    misc_deregister(&counter_device);
    // Ghi sự kiện nạp/gỡ hoặc trạng thái driver vào dmesg để kiểm chứng.
    pr_info("counter_driver: cleanup complete\n");
}
// Đăng ký hàm được kernel gọi khi insmod nạp module.
module_init(counter_init);
// Đăng ký hàm dọn tài nguyên được gọi khi rmmod gỡ module.
module_exit(counter_exit);
// Khai báo giấy phép GPL cho module.
MODULE_LICENSE("GPL");
// Ghi họ tên và MSSV được cung cấp, hiển thị bằng modinfo.
MODULE_AUTHOR("Phạm Võ Hoàng Nhân - SE200778");
// Mô tả chức năng module để nhận diện trong modinfo.
MODULE_DESCRIPTION("ASM1 atomic counter misc device with per-open EOF");
// Khai báo phiên bản triển khai 1.0.
MODULE_VERSION("1.0");
