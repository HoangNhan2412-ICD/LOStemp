
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
// Cung cấp waitpid và macro kiểm tra trạng thái tiến trình con.
#include <sys/wait.h>
// Cung cấp số syscall dùng để kiểm tra con trỏ lỗi trực tiếp.
#include <sys/syscall.h>
// Macro test: điều kiện sai thì in dòng lỗi/errno và thoát 1; đúng thì in PASS.
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s errno=%d\n",__LINE__,#x,errno); exit(1); } printf("PASS %s\n",#x); } while (0)

// Đọc procfs, ghi ba số vào con trỏ đầu ra; không trả giá trị, dừng test nếu lỗi.
static void stats(int *value,int *reads,int *writes)
{
    // Mở file thống kê counter ở chế độ chỉ đọc.
    FILE *f = fopen("/proc/counter_info","r");
    // Kiểm tra handle hoặc PID hợp lệ trước khi thực hiện bước tiếp theo.
    CHECK(f != NULL);
    // Xác nhận đọc đủ ba trường có nhãn từ file procfs.
    CHECK(fscanf(f,"counter: %d\nread_count: %d\nwrite_count: %d",value,reads,writes) == 3);
    // Đóng stream procfs sau khi đọc thống kê.
    fclose(f);
    // In dữ liệu thực tế hoặc thông tin ABI để lưu làm minh chứng kiểm thử.
    printf("COUNTER value=%d reads=%d writes=%d\n",*value,*reads,*writes);
}

// Điểm bắt đầu chương trình test, không nhận tham số; trả 0 nếu mọi kiểm tra đạt.
int main(void)
{
    // Mở counter đọc/ghi và khai báo biến lưu giá trị, thống kê, trạng thái con.
    int fd = open("/dev/counter",O_RDWR), i, v,r,w,status;
    // Buffer nhận chuỗi và một byte phụ để kiểm tra EOF.
    char buf[32]={0}, byte;
    // Lưu PID của bốn tiến trình con để đợi và kiểm tra từng tiến trình.
    pid_t children[4];
    // Kiểm tra handle hoặc PID hợp lệ trước khi thực hiện bước tiếp theo.
    CHECK(fd >= 0);
    // Đối chiếu số byte ghi thực tế với số byte mong đợi trong điều kiện.
    CHECK(write(fd,"100\n",4) == 4);
    // Đối chiếu số byte đọc; kết quả 0 kiểm chứng EOF hoặc yêu cầu độ dài 0.
    CHECK(read(fd,buf,0) == 0);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(syscall(SYS_read,fd,(void *)1,4) == -1 && errno == EFAULT);
    // Đối chiếu số byte đọc; kết quả 0 kiểm chứng EOF hoặc yêu cầu độ dài 0.
    CHECK(read(fd,buf,1) == 1);
    // Đối chiếu số byte đọc; kết quả 0 kiểm chứng EOF hoặc yêu cầu độ dài 0.
    CHECK(read(fd,buf+1,31) == 3);
    // Kiểm tra nội dung chuỗi hoặc byte NUL nhận được từ driver.
    CHECK(!strcmp(buf,"100\n"));
    // Đối chiếu số byte đọc; kết quả 0 kiểm chứng EOF hoặc yêu cầu độ dài 0.
    CHECK(read(fd,&byte,1) == 0);
    // Đọc counter, số lượt đọc và số lượt ghi từ procfs.
    stats(&v,&r,&w);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(v == 101);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(write(fd,"-1",2) == -1 && errno == EINVAL);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(write(fd,"abc",3) == -1 && errno == EINVAL);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(write(fd,"1x",2) == -1 && errno == EINVAL);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(write(fd,"1\0x",3) == -1 && errno == EINVAL);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(write(fd,"2147483648",10) == -1 && errno == EINVAL);
    // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
    CHECK(syscall(SYS_write,fd,(void *)1,4) == -1 && errno == EFAULT);
    // Đối chiếu số byte ghi thực tế với số byte mong đợi trong điều kiện.
    CHECK(write(fd,"2147483647",10) == 10);
    {
        // Mở counter mới để lấy một snapshot độc lập.
        int rd=open("/dev/counter",O_RDONLY);
        // Kiểm tra handle hoặc PID hợp lệ trước khi thực hiện bước tiếp theo.
        CHECK(rd >= 0);
        // Xác nhận thao tác thất bại với đúng mã errno ghi trong điều kiện.
        CHECK(read(rd,buf,sizeof(buf)) == -1 && errno == EOVERFLOW);
        // Đóng descriptor đọc và giải phóng context nếu driver có cấp phát.
        close(rd);
    }
    // Đối chiếu số byte ghi thực tế với số byte mong đợi trong điều kiện.
    CHECK(write(fd,"0",1) == 1);
    // Đọc counter, số lượt đọc và số lượt ghi từ procfs.
    stats(&v,&r,&w);
    // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
    CHECK(v == 0);
    // Đẩy hết output trước fork, tránh process con in lặp buffer của process cha.
    fflush(NULL);
    // Lặp qua bốn tiến trình con để tạo hoặc thu kết quả tương ứng.
    for(i=0;i<4;i++) {
        // Tạo tiến trình con, lưu PID trong tiến trình cha.
        children[i]=fork();
        // Kiểm tra handle hoặc PID hợp lệ trước khi thực hiện bước tiếp theo.
        CHECK(children[i]>=0);
        // Nhánh chỉ chạy trong process con vì fork trả 0 tại đó.
        if(!children[i]) {
            // Biến đếm vòng lặp trong process con.
            int j;
            // Mỗi process con đọc 20 snapshot độc lập.
            for(j=0;j<20;j++) {
                // Mở counter mới để lấy một snapshot độc lập.
                int rd=open("/dev/counter",O_RDONLY);
                // Kiểm tra mở thành công, có dữ liệu và lần đọc tiếp trả EOF; lỗi thì thoát mã 2.
                if(rd<0 || read(rd,buf,sizeof(buf))<=0 || read(rd,&byte,1)!=0) _exit(2);
                // Đóng descriptor đọc và giải phóng context nếu driver có cấp phát.
                close(rd);
            }
            // Kết thúc process con thành công mà không flush lại stdio của cha.
            _exit(0);
        }
    }
    // Lặp qua bốn tiến trình con để tạo hoặc thu kết quả tương ứng.
    for(i=0;i<4;i++) {
        // Đợi đúng process con kết thúc, tránh bỏ sót kết quả kiểm thử đồng thời.
        CHECK(waitpid(children[i],&status,0)==children[i]);
        // Xác nhận process con thoát bình thường với mã thành công 0.
        CHECK(WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    {
        // Lưu ba số thống kê sau khi tất cả process con kết thúc.
        int endv,endr,endw;
        // Đọc thống kê cuối để xác nhận tăng đủ 80 lần và không đổi số lượt ghi.
        stats(&endv,&endr,&endw);
        // Đối chiếu giá trị thực tế với điều kiện mong đợi; sai thì dừng test và báo lỗi.
        CHECK(endv==80 && endr==r+80 && endw==w);
    }
    // Đóng descriptor để trả tài nguyên và tham chiếu module.
    close(fd);
    // In marker chỉ sau khi toàn bộ kiểm tra trước đó đã đạt.
    puts("TASK2_C_TESTS_PASS");
    // Báo thành công hoặc không có byte dữ liệu, tùy hợp đồng của hàm hiện tại.
    return 0;
}
