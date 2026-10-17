
// Cung cấp printf, fprintf và thao tác đọc file thống kê.
#include <stdio.h>
// Cung cấp exit để dừng kiểm thử khi phát hiện lỗi.
#include <stdlib.h>
// Cung cấp so sánh, tìm kiếm và khởi tạo vùng nhớ.
#include <string.h>
// Khai báo errno và các mã lỗi để kiểm tra kết quả syscall.
#include <errno.h>
// Khai báo open và cờ O_RDONLY, O_WRONLY, O_RDWR.
#include <fcntl.h>
// Khai báo read, write, close, fork và _exit.
#include <unistd.h>
// Khai báo hàm ioctl được chương trình userspace gọi.
#include <sys/ioctl.h>
// Cung cấp waitpid và macro kiểm tra trạng thái tiến trình con.
#include <sys/wait.h>
// Cung cấp số syscall dùng để kiểm tra con trỏ lỗi trực tiếp.
#include <sys/syscall.h>
// Dùng chung định nghĩa command và cấu trúc dữ liệu ioctl với ứng dụng.
#include "asgn1_ioctl.h"
// Macro test: điều kiện sai thì in dòng lỗi/errno và thoát 1; đúng thì in PASS.
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s (errno=%d)\n",__LINE__,#x,errno); exit(1); } printf("PASS %s\n",#x); } while (0)

// Mở /dev/asgn1 theo flags; trả fd hợp lệ, dừng test nếu mở thất bại.
static int device(int flags)
{
    // Mở device với quyền do đối số flags quyết định.
    int fd = open("/dev/asgn1", flags);
    // Kiểm tra handle hoặc PID hợp lệ trước khi thực hiện bước tiếp theo.
    CHECK(fd >= 0);
    // Trả descriptor cho bên gọi.
    return fd;
}

// Gọi GET_STATS trên fd, in đủ năm trường và trả struct đã nhận.
static struct asgn1_stats get_stats(int fd)
{
    // Dành chỗ cho struct thống kê trả về từ kernel.
    struct asgn1_stats s;
    // Xác nhận ioctl trả thành công; tên command trong điều kiện chỉ rõ thao tác đang thử.
    CHECK(ioctl(fd, ASGN1_GET_STATS, &s) == 0);
    // In dữ liệu thực tế hoặc thông tin ABI để lưu làm minh chứng kiểm thử.
    printf("STATS open_count=%d write_count=%d read_count=%d buffer_len=%d last_write_size=%d\n",
           // Truyền các giá trị cần in tương ứng với chuỗi định dạng ở dòng trước.
           s.open_count,s.write_count,s.read_count,s.buffer_len,s.last_write_size);
    // Trả bản sao thống kê cho test tiếp theo.
    return s;
}

// Mở descriptor mới, đối chiếu chuỗi expected và EOF; không trả giá trị.
static void expect_text(const char *expected)
{
    // Buffer nhận dữ liệu được điền 0 để so sánh chuỗi an toàn.
    char text[ASGN1_CAPACITY + 1] = {0};
    // Mở descriptor chỉ đọc với offset bắt đầu bằng 0.
    int fd = device(O_RDONLY);
    // Đọc dữ liệu vào buffer, chừa byte kết thúc chuỗi.
    ssize_t n = read(fd, text, sizeof(text)-1);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(n == (ssize_t)strlen(expected));
    // Kiểm tra nội dung chuỗi hoặc byte NUL nhận được từ driver.
    CHECK(!strcmp(text, expected));
    // Đối chiếu số byte đọc; kết quả 0 kiểm chứng EOF hoặc yêu cầu độ dài 0.
    CHECK(read(fd, text, sizeof(text)) == 0);
    // Đóng descriptor để trả tài nguyên và tham chiếu module.
    close(fd);
}

// Điểm bắt đầu chương trình test, không nhận tham số; trả 0 nếu mọi kiểm tra đạt.
int main(void)
{
    // Mở device đọc/ghi và khai báo mode, chỉ số vòng lặp, trạng thái process con.
    int fd = device(O_RDWR), mode, i, status;
    // Lưu thống kê hiện tại và mốc trước khi chạy test đồng thời.
    struct asgn1_stats s, before;
    // Đặt 8 byte canh sau vùng version để phát hiện ghi vượt 64 byte.
    struct { char text[64]; unsigned char guard[8]; } version;
    // Chuẩn bị dữ liệu dùng kiểm thử ranh giới dung lượng buffer.
    char large[ASGN1_CAPACITY];
    // Lưu PID của bốn tiến trình con để đợi và kiểm tra từng tiến trình.
    pid_t children[4];
    // In dữ liệu thực tế hoặc thông tin ABI để lưu làm minh chứng kiểm thử.
    printf("ABI stats_size=%zu GET_STATS=0x%lx nr=%u size=%u\n", sizeof(s),
           // Truyền các giá trị cần in tương ứng với chuỗi định dạng ở dòng trước.
           (unsigned long)ASGN1_GET_STATS, (unsigned)_IOC_NR(ASGN1_GET_STATS), (unsigned)_IOC_SIZE(ASGN1_GET_STATS));
    // Lấy snapshot thống kê hiện tại qua ioctl.
    s = get_stats(fd);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.open_count == 1 && s.write_count == 0 && s.read_count == 0 && s.buffer_len == 0);
    // Đối chiếu số byte ghi thực tế với số byte mong đợi trong điều kiện.
    CHECK(write(fd,"hello",5) == 5);
    // Xác nhận mode mặc định Echo trả nguyên chuỗi hello.
    expect_text("hello");
    // Lấy snapshot thống kê hiện tại qua ioctl.
    s = get_stats(fd);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.write_count == 1 && s.read_count == 1 && s.buffer_len == 5 && s.last_write_size == 5);
    // Chuẩn bị chọn Kernel mode có tiền tố.
    mode = 0;
    // Xác nhận ioctl trả thành công; tên command trong điều kiện chỉ rõ thao tác đang thử.
    CHECK(ioctl(fd,ASGN1_SET_MODE,&mode) == 0);
    // Đối chiếu số byte ghi thực tế với số byte mong đợi trong điều kiện.
    CHECK(write(fd,"abc",3) == 3);
    // Xác nhận driver đã thêm đúng tiền tố [KERNEL].
    expect_text("[KERNEL]abc");
    // Lấy snapshot thống kê hiện tại qua ioctl.
    s = get_stats(fd);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.buffer_len == 11 && s.last_write_size == 3);
    // Chuẩn bị chọn Echo mode lưu nguyên dữ liệu.
    mode = 1;
    // Xác nhận ioctl trả thành công; tên command trong điều kiện chỉ rõ thao tác đang thử.
    CHECK(ioctl(fd,ASGN1_SET_MODE,&mode) == 0);
    // Đối chiếu số byte ghi thực tế với số byte mong đợi trong điều kiện.
    CHECK(write(fd,"echo",4) == 4);
    // Xác nhận dữ liệu hiện tại vẫn đúng chuỗi echo.
    expect_text("echo");
    // Điền mẫu 0xa5 vào version và vùng canh để phát hiện ghi vượt giới hạn.
    memset(&version,0xa5,sizeof(version));
    // Xác nhận ioctl trả thành công; tên command trong điều kiện chỉ rõ thao tác đang thử.
    CHECK(ioctl(fd,ASGN1_GET_VERSION,version.text) == 0);
    // Kiểm tra nội dung chuỗi hoặc byte NUL nhận được từ driver.
    CHECK(memchr(version.text,0,64) != NULL);
    // Kiểm tra từng byte canh không bị GET_VERSION ghi đè.
    for(i=0;i<8;i++) CHECK(version.guard[i] == 0xa5);
    // In dữ liệu thực tế hoặc thông tin ABI để lưu làm minh chứng kiểm thử.
    printf("VERSION %s\n",version.text);
    // Tạo mode ngoài tập 0/1 để kiểm tra driver từ chối.
    mode = 2;
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(ioctl(fd,ASGN1_SET_MODE,&mode) == -1 && errno == EINVAL);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(ioctl(fd,_IO(ASGN1_MAGIC,99),0) == -1 && errno == ENOTTY);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(ioctl(fd,ASGN1_GET_STATS,(void *)1) == -1 && errno == EFAULT);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(ioctl(fd,ASGN1_SET_MODE,(void *)1) == -1 && errno == EFAULT);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(ioctl(fd,ASGN1_GET_VERSION,(void *)1) == -1 && errno == EFAULT);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(ioctl(fd,_IOR(ASGN1_MAGIC,1,char),&s) == -1 && errno == ENOTTY);
    // Tạo buffer đầy ký tự x cho test kích thước biên.
    memset(large,'x',sizeof(large));
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(write(fd,large,sizeof(large)) == -1 && errno == EMSGSIZE);
    // Xác nhận dữ liệu hiện tại vẫn đúng chuỗi echo.
    expect_text("echo");
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(syscall(SYS_write,fd,(void *)1,4) == -1 && errno == EFAULT);
    // Xác nhận dữ liệu hiện tại vẫn đúng chuỗi echo.
    expect_text("echo");
    // Đối chiếu số byte ghi thực tế với số byte mong đợi trong điều kiện.
    CHECK(write(fd,large,sizeof(large)-1) == sizeof(large)-1);
    // Lấy snapshot thống kê hiện tại qua ioctl.
    s = get_stats(fd);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.buffer_len == 1023);
    // Chuẩn bị chọn Kernel mode có tiền tố.
    mode = 0;
    // Xác nhận ioctl trả thành công; tên command trong điều kiện chỉ rõ thao tác đang thử.
    CHECK(ioctl(fd,ASGN1_SET_MODE,&mode) == 0);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(write(fd,large,1016) == -1 && errno == EMSGSIZE);
    // Đối chiếu số byte ghi thực tế với số byte mong đợi trong điều kiện.
    CHECK(write(fd,large,1015) == 1015);
    // Lấy snapshot thống kê hiện tại qua ioctl.
    s = get_stats(fd);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.buffer_len == 1023 && s.last_write_size == 1015);
    // Xác nhận ioctl trả thành công; tên command trong điều kiện chỉ rõ thao tác đang thử.
    CHECK(ioctl(fd,ASGN1_RESET_BUFFER,0) == 0);
    // Lấy snapshot thống kê hiện tại qua ioctl.
    s = get_stats(fd);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.write_count == 0 && s.read_count == 0 && s.buffer_len == 0 && s.last_write_size == 0);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.open_count > 0);
    // Xác nhận buffer rỗng sau RESET.
    expect_text("");
    // Chuẩn bị chọn Echo mode lưu nguyên dữ liệu.
    mode = 1;
    // Xác nhận ioctl trả thành công; tên command trong điều kiện chỉ rõ thao tác đang thử.
    CHECK(ioctl(fd,ASGN1_SET_MODE,&mode) == 0);
    // Đối chiếu số byte ghi thực tế với số byte mong đợi trong điều kiện.
    CHECK(write(fd,"abcdef",6) == 6);
    {
        // Mở descriptor mới để kiểm tra đọc từng phần từ offset 0.
        int rd = device(O_RDONLY);
        // Buffer ghép các lần đọc ngắn, có NUL sẵn ở phần chưa ghi.
        char small[8] = {0};
        // Đối chiếu số byte đọc; kết quả 0 kiểm chứng EOF hoặc yêu cầu độ dài 0.
        CHECK(read(rd,small,2) == 2);
        // Đối chiếu số byte đọc; kết quả 0 kiểm chứng EOF hoặc yêu cầu độ dài 0.
        CHECK(read(rd,small+2,4) == 4);
        // Kiểm tra nội dung chuỗi hoặc byte NUL nhận được từ driver.
        CHECK(!strcmp(small,"abcdef"));
        // Đối chiếu số byte đọc; kết quả 0 kiểm chứng EOF hoặc yêu cầu độ dài 0.
        CHECK(read(rd,small,1) == 0);
        // Đóng descriptor đọc và giải phóng context nếu driver có cấp phát.
        close(rd);
    }
    // Lưu mốc thống kê để tính số thao tác đồng thời thực tế.
    before = get_stats(fd);
    // Đẩy hết output trước fork, tránh process con in lặp buffer của process cha.
    fflush(NULL);
    // Lặp qua bốn tiến trình con để tạo hoặc thu kết quả tương ứng.
    for(i=0;i<4;i++) {
        // Tạo tiến trình con, lưu PID trong tiến trình cha.
        children[i] = fork();
        // Kiểm tra handle hoặc PID hợp lệ trước khi thực hiện bước tiếp theo.
        CHECK(children[i] >= 0);
        // Nhánh chỉ chạy trong process con vì fork trả 0 tại đó.
        if(!children[i]) {
            // Mở descriptor ghi riêng cho process con và khai báo biến đếm.
            int j, c = open("/dev/asgn1",O_WRONLY);
            // Dừng process con với mã lỗi 2 nếu không mở được device.
            if(c < 0) _exit(2);
            // Ghi 100 lần, mỗi lần 4 byte; dừng lỗi 3 nếu thiếu byte hoặc syscall lỗi.
            for(j=0;j<100;j++) if(write(c,"race",4)!=4) _exit(3);
            // Đóng descriptor ghi riêng của process con.
            close(c);
            // Kết thúc process con thành công mà không flush lại stdio của cha.
            _exit(0);
        }
    }
    // Lặp qua bốn tiến trình con để tạo hoặc thu kết quả tương ứng.
    for(i=0;i<4;i++) {
        // Đợi đúng process con kết thúc, tránh bỏ sót kết quả kiểm thử đồng thời.
        CHECK(waitpid(children[i],&status,0) == children[i]);
        // Xác nhận process con thoát bình thường với mã thành công 0.
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    // Lấy snapshot thống kê hiện tại qua ioctl.
    s = get_stats(fd);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.write_count == before.write_count+400);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.open_count == before.open_count+4);
    // Xác nhận ioctl trả thành công; tên command trong điều kiện chỉ rõ thao tác đang thử.
    CHECK(ioctl(fd,ASGN1_RESET_BUFFER,0) == 0);
    // Lấy snapshot thống kê hiện tại qua ioctl.
    s = get_stats(fd);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(s.buffer_len == 0 && s.write_count == 0 && s.read_count == 0);
    // Đóng descriptor để trả tài nguyên và tham chiếu module.
    close(fd);
    // In marker chỉ sau khi toàn bộ kiểm tra trước đó đã đạt.
    puts("TASK1_C_TESTS_PASS");
    // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
    return 0;
}
