#!/usr/bin/env python3
"""Render a Vietnamese technical report from inspected code and real QEMU logs.
Requires reportlab installed locally under practice/environment/python-packages.
"""
from pathlib import Path
import hashlib
import html
import json
import re
import sys
import textwrap

P = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'environment/python-packages'))
from reportlab.pdfgen import canvas
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import Paragraph, Table, TableStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

S = P/'submission'
OUT = S/'bao_cao'
OUT.mkdir(exist_ok=True)
pdfmetrics.registerFont(TTFont('DV','/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf'))
pdfmetrics.registerFont(TTFont('DVB','/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf'))
pdfmetrics.registerFont(TTFont('Mono','/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf'))
STYLE=ParagraphStyle('body',fontName='DV',fontSize=12,leading=17,spaceAfter=9)
result=json.loads((S/'logs/result.json').read_text())
assert result['task1'] and result['task2'] and result['dyk_kernel'] and result['originals_unchanged']
def clean(text):
    """Strip ANSI colour codes for PDF only; raw log files remain unchanged."""
    return re.sub(r'\x1b\[[0-9;]*m','',text)
task1=clean((S/'logs/test_asgn1_output.txt').read_text())
task2=clean((S/'logs/test_counter_output.txt').read_text())
runtime=clean((S/'logs/runtime.txt').read_text())
dmesg=clean((S/'logs/dmesg_full.txt').read_text())
pdf=OUT/'SE200778_Asgn01_BaoCao.pdf'
c=canvas.Canvas(str(pdf),pagesize=(595.28,841.89))
c.setTitle('ASM1 LOS201 — Phạm Võ Hoàng Nhân — SE200778')
c.setAuthor('Phạm Võ Hoàng Nhân — SE200778')
page=0
y=0
md=[]

def start(title):
    """Start a numbered A4 page and reset vertical position."""
    global page,y
    if page: c.showPage()
    page+=1
    c.setFillColor(colors.HexColor('#17324d'))
    c.setFont('DVB',16)
    c.drawString(42,795,title)
    c.setStrokeColor(colors.HexColor('#168b91')); c.line(42,783,553,783)
    c.setFillColor(colors.black)
    c.setFont('DV',9)
    c.drawString(42,25,'LOS201 • ASM1 • Phạm Võ Hoàng Nhân • SE200778')
    c.drawRightString(553,25,str(page))
    y=763
    md.append('\n# '+title+'\n')

def para(text):
    """Draw wrapped 12pt paragraph, reject page overflow; also save Markdown."""
    global y
    p=Paragraph(html.escape(text),STYLE)
    _,h=p.wrap(511,720)
    if y-h<48: raise RuntimeError(f'Page {page} overflow: {text[:50]}')
    p.drawOn(c,42,y-h); y-=h+10
    md.append(text+'\n')

def table(rows,widths):
    """Draw table with wrapped 12pt cells and preserve its text in Markdown."""
    global y
    cells=[[Paragraph(html.escape(str(x)),STYLE) for x in row] for row in rows]
    t=Table(cells,colWidths=widths,hAlign='LEFT')
    t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),colors.HexColor('#e6eff6')),
                          ('VALIGN',(0,0),(-1,-1),'TOP'),('GRID',(0,0),(-1,-1),.4,colors.grey),
                          ('LEFTPADDING',(0,0),(-1,-1),6),('RIGHTPADDING',(0,0),(-1,-1),6),
                          ('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5)]))
    _,h=t.wrap(511,700)
    if y-h<48: raise RuntimeError(f'Table page {page} overflow')
    t.drawOn(c,42,y-h);y-=h+12
    md.extend([' | '.join(map(str,row)) for row in rows]);md.append('')

def code(text):
    """Draw actual source/log excerpt in 12pt monospace, wrapping visibly."""
    global y
    for line in text.splitlines():
        lines=textwrap.wrap(line,70,replace_whitespace=False,drop_whitespace=False) or ['']
        for part in lines:
            if y<62: raise RuntimeError(f'Code page {page} overflow')
            c.setFont('Mono',12);c.drawString(42,y-12,part);y-=15
    y-=8
    md.append('```text\n'+text+'\n```\n')

start('1. Phạm vi, tác giả và môi trường')
para('BÁO CÁO PHÂN TÍCH & THIẾT KẾ DEVICE DRIVER — ASSIGNMENT 01')
para('Họ tên: Phạm Võ Hoàng Nhân. MSSV: SE200778. Môn học: LOS201 — Embedded Linux Systems. Bài tập: How to Write a Device Driver, nội dung liên quan Chapter 8–10.')
para('Phạm vi báo cáo gồm thiết kế hai driver, giao diện ioctl và misc device, quy trình build ARM, kiểm thử trên QEMU và phân tích các quyết định kỹ thuật. Mã nguồn có chú thích tiếng Việt để giải thích dữ liệu, tham số, giá trị trả về, khóa đồng bộ và các trường hợp lỗi. Các kết luận chức năng được đối chiếu với log thực tế đi kèm.')
table([['Thành phần','Giá trị đã kiểm tra'],['Host','AlmaLinux 10.2, x86_64, Bash'],['Guest','QEMU vexpress-a9, Cortex-A9, 512 MB, 2 CPU'],['Kernel chạy','Linux 5.15.0, dyk@mydklaptop, GCC 15.2.0'],['Compiler module','Arm GNU Toolchain 13.2.1; arm-none-linux-gnueabihf-'],['Nguồn build','Linux 5.15.0 nguyên bản; cấu hình kernel dyk đã đối chiếu SHA-256']],[145,366])
para('Môi trường thực hành được tổ chức trong practice/, tách biệt với các file mẫu. Host AlmaLinux dùng để biên dịch chéo; module ARM được nạp và gỡ trong guest QEMU. Cách tổ chức này giúp giữ nguyên tài liệu gốc và truy vết riêng kết quả build, kiểm thử.')
para('Sản phẩm kỹ thuật: hai driver và header ioctl, Makefile, hai executable kiểm thử ARM liên kết tĩnh, script shell, log thật, PDF và file tar.gz. Hồ sơ build và lịch sử lần chạy lỗi cũng được giữ để truy vết.')

start('2. Kiến trúc chung và dòng dữ liệu')
def box(x,yy,w,h,label):
    """Draw original architecture node; label is a list of short lines."""
    c.setFillColor(colors.HexColor('#eef5f8'));c.roundRect(x,yy,w,h,5,fill=1,stroke=1)
    c.setFillColor(colors.black);c.setFont('DV',12)
    for i,line in enumerate(label): c.drawCentredString(x+w/2,yy+h-19-i*16,line)
def arrow(x1,y1,x2,y2):
    """Draw directed vertical/horizontal arrow between architecture nodes."""
    c.line(x1,y1,x2,y2)
    if y2<y1: c.line(x2,y2,x2-4,y2+7);c.line(x2,y2,x2+4,y2+7)
    else: c.line(x2,y2,x2-7,y2-4);c.line(x2,y2,x2-7,y2+4)
box(110,700,375,45,['Userspace: test C, shell cat/echo'])
box(110,626,375,48,['open / read / write / ioctl syscall','VFS → file_operations'])
arrow(297,700,297,674)
box(42,509,245,78,['asgn1_driver /dev/asgn1','unlocked_ioctl + read/write','mutex → buffer + stats'])
box(308,509,245,78,['counter_driver /dev/counter','read/write + per-open snapshot','mutex + atomic_t counter'])
arrow(180,626,180,587);arrow(420,626,420,587)
box(42,419,245,53,['/proc/asgn1_info','snapshot dưới mutex'])
box(308,419,245,53,['/proc/counter_info','/sys/class/misc/counter'])
arrow(180,509,180,472);arrow(420,509,420,472)
box(42,350,511,50,['ioctl() → unlocked_ioctl → xử lý',
                      'copy_to/from_user ↔ userspace'])
y=330
md.append('Sơ đồ: userspace → syscall/VFS → hai nhánh file_operations → buffer/counter → procfs; counter có sysfs qua misc framework.\n')
para('Dòng ioctl: chương trình C gọi ioctl(fd, cmd, arg) → syscall → VFS → asgn1_fops.unlocked_ioctl → kiểm tra command → xử lý trạng thái. SET_MODE dùng copy_from_user; GET_STATS và GET_VERSION dùng copy_to_user. Kết quả và mã lỗi quay về userspace.')
para('Shell không tự định nghĩa ABI ioctl; test_asgn1.sh gọi executable asgn1_test dùng chung asgn1_ioctl.h. Counter độc lập, đăng ký bằng misc_register. /sys/class/misc/counter là sysfs do framework tạo; không tuyên bố Task 1 có sysfs attributes riêng.')
para('Data layer gồm buffer/thống kê và atomic counter. Operation layer thực hiện khóa, parse và cập nhật. Interface layer là file_operations, unlocked_ioctl, procfs và chương trình userspace. Sơ đồ mô tả hai driver cùng các giao diện thực sự có trong bản triển khai.')

start('3. Thiết kế Task 1 — ioctl và buffer')
table([['Command','Direction / size','Hành vi'],['RESET_BUFFER','_IO / 0','Xóa buffer; reset read/write count, length và last size; giữ open_count và mode.'],['GET_STATS','_IOR / 20','Năm int 32-bit; snapshot dưới mutex rồi copy_to_user.'],['SET_MODE','_IOW / 4','0 thêm [KERNEL]; 1 Echo. Giá trị khác: EINVAL.'],['GET_VERSION','_IOR / 64','Chuỗi có SE200778; mảng khởi tạo đủ 64 byte, không lộ dữ liệu stack.']],[130,115,266])
para('Magic là ký tự A (0x41); số command 0–3. Trên ARM EABI dùng ở đây, GET_STATS = 0x80144101, _IOC_NR = 1, _IOC_SIZE = 20; _IOC_DIR = 2 nghĩa là đọc từ kernel về userspace. Size truyền vào macro là kiểu struct, không phải sizeof(struct). Magic giúp phân nhóm, không phải cơ chế bảo mật hay đăng ký độc quyền toàn hệ thống. [1]')
para('Major cố định 241, minor 0; script tạo /dev/asgn1. register_chrdev đăng ký fops, open từ chối minor khác. open_count là số lần mở thành công tích lũy, không phải số descriptor đang mở; read_count chỉ tăng khi thực sự trả dữ liệu. Các bộ đếm bão hòa tại INT_MAX để tránh signed overflow.')
para('Buffer 1024 byte giữ tối đa 1023 byte dữ liệu. Kernel mode dành 8 byte cho [KERNEL], nên nhận tối đa 1015 byte. Ghi quá dài trả EMSGSIZE thay vì cắt âm thầm. memdup_user tạo bản tạm; copy lỗi không phá dữ liệu cũ. memcpy chỉ dùng giữa hai vùng kernel, tuyệt đối không thay copy_to_user trong ioctl.')
para('Tất cả truy cập buffer, stats và mode đều được bảo vệ bằng cùng mutex. RESET không đổi mode/open_count vì đề chỉ yêu cầu reset buffer và read/write count. Mutex được phép ngủ khi xử lý uaccess; không giữ spinlock qua một phép copy có thể page fault. read dùng offset và simple_read_from_buffer để có EOF.')

start('4. Thiết kế Task 2 — misc counter')
para('counter_driver được viết độc lập với Lab 02. struct miscdevice dùng MISC_DYNAMIC_MINOR, major 10 và tên counter. misc_register đơn giản hóa đăng ký thiết bị; trong guest có devtmpfs nên /dev/counter xuất hiện. proc_create bổ sung /proc/counter_info; nếu tạo proc thất bại, init hủy đăng ký misc ngay.')
para('counter, reads và writes dùng atomic_t. Mutex bảo vệ giao dịch lấy giá trị → tạo snapshot → tăng counter, đồng thời phối hợp với thao tác set. atomic_read rồi atomic_inc riêng lẻ không tự tạo thành một giao dịch: hai CPU có thể cùng quan sát giá trị 5 trước khi tăng. Mutex tránh trả trùng snapshot trong trường hợp đó. [3]')
para('Mỗi lần open có read_context riêng. Lần read đầu thành công chụp số, tăng counter một lần; các lần read ngắn tiếp theo trả phần còn lại của cùng chuỗi. Khi offset đạt độ dài, trả 0. Read count=0 hay EFAULT không làm tăng counter. release giải phóng context. Ghi trên descriptor đã đọc không tự tua lại offset; mở lại để đọc snapshot mới.')
para('write nhận ASCII, kiểm tra độ dài và NUL nhúng, dùng kstrtoint(base 10). Số âm, ký tự dư hoặc vượt int trả EINVAL; ghi hợp lệ dùng atomic_set, tăng write_count. write_count tính mọi lần set thành công, kể cả set 100; tên reset trong rubric được hiểu theo đặc tả mục 3.7. Tại INT_MAX, read trả EOVERFLOW để không wrap sang số âm; đây là chính sách biên bổ sung được ghi rõ.')
table([['Thời điểm thực tế','counter','reads','writes'],['Trước write','0','0','0'],['Sau echo 100','100','2','1'],['Sau echo 0','0','3','2']],[251,85,85,90])
para('Các số trên lấy từ logs/test_counter_output.txt, theo đúng thứ tự script: đọc 0, đọc 1, set 100, đọc 100, reset 0. /proc đọc thống kê không tự tăng counter. Test bổ sung chạy 4 process, mỗi process 20 lần open/read/close: tổng tăng 80, không mất increment.')

start('5. So sánh ba loại driver')
table([['Tiêu chí','Character','Block','Network'],['Giao diện','file_operations cho open/read/write/ioctl','block_device_operations quản lý thiết bị; bio/blk-mq cho I/O','net_device_ops; socket là giao diện ứng dụng'],['Đơn vị','Byte stream hoặc bản ghi do driver quy định','Block/sector có địa chỉ','Packet/frame'],['Buffering','Tùy driver; không mặc định có page cache như file thường','Page cache khi buffered I/O, hàng đợi block; direct I/O có thể bỏ qua cache','sk_buff, queue TX/RX, socket buffer, NAPI'],['Ví dụ nhúng','UART, cảm biến điều khiển, counter','eMMC, SSD, SD có controller','Ethernet MAC, Wi-Fi']],[91,140,140,140])
para('VFS không gọi net_device_ops theo cùng cách gọi read của character device. Ứng dụng dùng socket; network stack chuyển packet tới driver. Tương tự, block_device_operations không phải là đường dữ liệu duy nhất của block driver: bio/request và blk-mq xử lý I/O. Bảng giữ các thuật ngữ đề yêu cầu nhưng tránh đồng nhất ba mô hình.')
para('Quyết định 1: thiết bị cảm biến cần đọc vài byte và đổi chế độ đo phù hợp character/misc với read và ioctl; dựng block device sẽ áp đặt sector, địa chỉ và cache không cần thiết. Counter của bài là ví dụ đơn mục đích phù hợp misc.')
para('Quyết định 2: lưu log dạng filesystem trên eMMC dùng block subsystem vì cần truy cập các block và phối hợp filesystem/cache. Nếu thiết bị là Ethernet MAC truyền frame giữa các nút, chọn network subsystem để dùng routing, socket và các cơ chế hàng đợi hiện có.')
para('Task 1 dùng register_chrdev để minh họa major tự quản lý và ABI ioctl. Task 2 dùng misc_register để chia sẻ major 10 và cấp minor động. Misc giảm mã đăng ký/devnode nhưng phù hợp thiết bị nhỏ; character registration riêng linh hoạt hơn khi quản lý nhiều minor hoặc tổ chức lớp thiết bị riêng.')

start('6. Kiểm thử và giới hạn tương thích')
para('Kết quả lấy từ một lần chạy QEMU mới, không nhập lại bằng tay: logs/result.json báo task1=true, task2=true, dyk_kernel=true, completed=true, no_kernel_oops=true và originals_unchanged=true. Việc nạp module không dùng tùy chọn force. Full serial và initramfs của từng lần chạy nằm dưới practice/output/runs/.')
table([['Nhóm kiểm tra','Bằng chứng và kết quả'],['Task 1','Default Echo, Kernel mode, trở lại Echo; 5 trường stats; RESET rồi cat rỗng; version có guard 8 byte; tất cả đạt.'],['Lỗi và biên Task 1','EINVAL mode sai; ENOTTY command/size sai; EFAULT pointer; EMSGSIZE; 1023 byte và 1015+8 byte; đọc từng phần/EOF; 400 write đồng thời.'],['Task 2','Đăng ký major 10; 0→1; set 100; reset 0; ba snapshot procfs; read ngắn và EOF; EINVAL/EFAULT; INT_MAX; 80 read đồng thời.'],['Vòng đời','Cả hai giữ module khi fd mở; đóng fd rồi gỡ; proc/sysfs được dọn; reload hoạt động.'],['Build/clean','make → clean → make trên bản sao; source còn nguyên, output được tạo lại; log build-clean-verification.txt.']],[135,376])
para('Cấu hình gốc khớp SHA-256 với /proc/config.gz. Compiler module là 13.2.1, compiler kernel dyk là 15.2.0. modules_prepare và vermagic chỉ là kiểm tra sơ bộ. Module.symvers dùng cho liên kết được tạo từ vmlinux build cục bộ, không mạo nhận là file do dyk cung cấp. CONFIG_MODVERSIONS tắt; bằng chứng quan trọng là insmod, chức năng và rmmod thực tế trên kernel dyk đều đạt. [2]')
para('Giới hạn: chưa chứng minh tương thích với mọi kernel 5.15, mọi tải hoặc mọi cấu hình. Không chạy KASAN/kmemleak hay fault injection cấp phát. Tài nguyên được rà soát theo từng nhánh init/exit và kiểm tra reload; không suy diễn “không rò rỉ trong mọi trường hợp”. Cảnh báo nền phần cứng QEMU vẫn được giữ trong dmesg; không có kernel Oops/panic trong lượt kiểm thử.')

start('7. Lỗi thực tế và cách sửa')
para('Lỗi 1 — build thất bại vì trùng asgn1_open. DEFINE_PROC_SHOW_ATTRIBUTE(asgn1) tự sinh một hàm asgn1_open cho procfs, trùng callback open của character device. Log gốc: logs/task1-build.txt. Sửa bằng namespace asgn1_status_show / DEFINE_PROC_SHOW_ATTRIBUTE(asgn1_status); áp dụng cùng nguyên tắc cho counter. Build lại thành công.')
code('error: redefinition of ‘asgn1_open’\nnote: in definition of macro ‘DEFINE_PROC_SHOW_ATTRIBUTE’')
para('Lỗi 2 — cảnh báo stack frame 1032 byte vượt ngưỡng 1024. Bản đầu đặt input[1024] trên kernel stack. Log: logs/task1-build-fixed.txt. Chuyển sang memdup_user và kfree ở cả đường thành công lẫn đường EMSGSIZE. Không giữ buffer tạm trên stack lớn; uaccess lỗi trả errno trước khi cập nhật trạng thái.')
para('Lỗi 3 — script báo thất bại dù kernel từ chối unload đúng. Trong lượt 20261017-021607, BusyBox rmmod in “Resource temporarily unavailable” nhưng nhánh if coi exit status là thành công. Hai chương trình C đã đạt, nhưng shell test kết luận sai. Không sửa driver để vượt kiểm tra refcount; sửa script để kiểm tra trạng thái có thật trong /proc/modules.')
code("rmmod asgn1_driver || true\ngrep '^asgn1_driver ' /proc/modules\nexec 9>&-\nrmmod asgn1_driver\nif grep -q '^asgn1_driver ' /proc/modules; then exit 1; fi")
para('Sau sửa, cả hai script có marker PASS, proc entry biến mất, counter sysfs/devnode biến mất và dmesg báo cleanup complete. Lượt lỗi vẫn được giữ bên cạnh lượt thành công; không xóa hay sửa log để che kết quả ban đầu.')
para('Khác biệt cấu hình cũng đã được xử lý: GCC 13.2 phát hiện plugin headers nên olddefconfig tự bật GCC_PLUGINS và STACKPROTECTOR_PER_TASK. Tắt GCC_PLUGINS trong bản thực hành đưa các tùy chọn chức năng về trạng thái của dyk; diff còn thông tin toolchain và dòng tắt plugin.')

start('8. Lý thuyết — reference count và uaccess')
para('Câu 1. Module có reference count để mã và dữ liệu không bị giải phóng khi còn người dùng. Với .owner = THIS_MODULE, lấy file_operations qua fops_get gọi try_module_get; giải phóng qua fops_put gọi module_put. Lần open tạo file object giữ tham chiếu; close cuối cùng mới giải phóng. dup/fork chia sẻ file object, không nhất thiết tăng module count đúng một lần cho mỗi descriptor. Phụ thuộc giữa module và tham chiếu tường minh cũng có thể giữ module.')
para('rmmod khi còn fd mở thường bị từ chối vì module còn được dùng; trong test kernel giữ module, BusyBox in Resource temporarily unavailable. Đóng fd rồi gỡ thành công. Về lý thuyết có rmmod -f nếu kernel bật CONFIG_MODULE_FORCE_UNLOAD và tiến trình có quyền cần thiết; cưỡng bức có thể để lại con trỏ vào mã đã giải phóng. Kernel dyk của bài tắt tùy chọn này, nên không thể dựa vào force unload. Không thực hiện force unload trong kiểm thử.')
para('Câu 2. Con trỏ userspace là địa chỉ ảo không đáng tin cậy, phụ thuộc address space hiện hành. MMU dịch địa chỉ qua page table và kiểm tra quyền; trang có thể chưa được ánh xạ, chỉ đọc, bị thay đổi hoặc cần xử lý page fault. memcpy thông thường không cung cấp hợp đồng uaccess và cơ chế sửa lỗi truy cập phù hợp để trả lỗi cho syscall.')
para('Không đúng khi khẳng định kernel “luôn không thể” truy cập địa chỉ user hoặc “luôn panic”: kết quả tùy mapping và cơ chế bảo vệ của kiến trúc. Truy cập trực tiếp có thể tình cờ hoạt động; cũng có thể gây translation/permission fault không được phục hồi đúng, dẫn tới Oops, hoặc ghi nhầm vùng nếu bỏ kiểm tra địa chỉ. copy_to_user dùng kiểm tra và đường exception fixup của kiến trúc, trả số byte chưa copy; driver đổi kết quả khác 0 thành -EFAULT.')
para('GET_STATS chụp struct dưới mutex rồi copy_to_user snapshot. Không trả con trỏ kernel cho userspace. Struct gồm 5 int, không có trường pointer hay dữ liệu padding chưa khởi tạo trong ABI ARM này. GET_VERSION khởi tạo cả mảng 64 byte trước khi copy. Test pointer 0x1 cho GET_STATS, SET_MODE và GET_VERSION trả EFAULT mà kernel vẫn chạy.')
para('Nguồn đối chiếu đúng Linux 5.15: include/linux/fs.h (fops_get/fops_put), kernel/module.c (try_stop_module, try_force_unload), include/linux/uaccess.h và arch/arm/include/asm/uaccess.h. Đây là giải thích kỹ thuật; không phải tuyên bố đã mô phỏng mọi lỗi MMU.')

start('9. Lý thuyết — MTD, offset và tài liệu')
para('Câu 3. MTD biểu diễn flash thô với erase block, yêu cầu erase trước program, giới hạn chu kỳ và các đặc điểm như bad block/OOB/ECC ở NAND. Block subsystem cung cấp địa chỉ sector và I/O block; thiết bị như eMMC/SSD có controller/FTL che việc quản lý flash nên thường đi qua block layer. Không phải cứ lưu trữ trên flash là phải dùng MTD.')
para('Ví dụ: chip SPI NOR chứa firmware hoặc raw NAND nối trực tiếp SoC cần MTD và driver/controller thích hợp. Raw NAND có thể dùng UBI/UBIFS để quản lý phía trên MTD. Thẻ SD/eMMC trình bày block interface thì dùng block layer. mtdblock là lớp chuyển tiếp, không tự biến raw flash thành ổ đĩa có đầy đủ FTL, wear leveling và độ tin cậy tương đương SSD. Task 4 chỉ yêu cầu giải thích; không chạy flash_erase/nandsim trên host.')
para('Câu 4. cat lặp syscall read cho tới khi nhận 0 (EOF) hoặc lỗi. Mỗi syscall đi qua VFS rồi callback read của driver; VFS không tự lặp vô hạn bên trong một lần gọi để thay cat. Nếu callback luôn trả chuỗi số với số byte dương, cat sẽ tiếp tục gọi và in dữ liệu mãi, có thể làm counter tăng liên tục.')
para('counter_read dùng snapshot riêng cho mỗi open và *ppos. Lần đọc đầu trả một phần hoặc toàn bộ chuỗi, tăng counter đúng một lần sau khi copy thành công. Các lần đọc sau trả phần còn lại; khi offset bằng độ dài thì trả 0. Điều này xử lý cả cat lẫn ứng dụng đọc từng byte. Test đã xác nhận “100\\n” được đọc theo 1+3 byte, lần kế tiếp EOF và counter chỉ tăng lên 101.')
para('Tài liệu sử dụng: đề ASSIGNMENT01_EmbeddedLinux.docx, mục 3.2–3.10 và IV–V; README/BUILD_FROM_SCRATCH của Lab 01/02 trong repository. Thiết kế mới có tham khảo khái niệm Lab 02, không sao chép code từ GitHub/Stack Overflow ngoài repository được cung cấp.')
para('[1] Linux Kernel Documentation, ioctl based interfaces: https://docs.kernel.org/driver-api/ioctl.html. [2] Building External Modules: https://docs.kernel.org/kbuild/modules.html. [3] Atomic types: https://docs.kernel.org/core-api/wrappers/atomic_t.html. Bản tài liệu đi kèm Linux 5.15 được ưu tiên khi API khác phiên bản hiện hành.')
para('Các nguồn mã bổ sung: drivers/char/misc.c; include/linux/atomic/; fs/read_write.c; fs/libfs.c; include/linux/mtd/mtd.h và Documentation/driver-api/mtd/. Các đường dẫn này nằm trong cây mã nguồn Linux 5.15 đã dùng để chuẩn bị môi trường build.')

# Full Task 1 log is embedded verbatim except ANSI colours and visual wrapping.
lines=[]
for line in task1.splitlines():
    lines.extend(textwrap.wrap(line,70,replace_whitespace=False,drop_whitespace=False) or [''])
for offset in range(0,len(lines),44):
    start('Phụ lục A — toàn bộ log Task 1')
    para(f'Nguồn: logs/test_asgn1_output.txt. Đoạn {offset//44+1}; chỉ bỏ mã màu và xuống dòng để vừa trang.')
    code('\n'.join(lines[offset:offset+44]))
start('Phụ lục B — minh chứng và gói bàn giao')
para('Các file sau chứa dữ liệu nguyên bản, không phải output giả định: logs/test_asgn1_output.txt, logs/test_counter_output.txt, logs/dmesg_full.txt, logs/runtime.txt, logs/result.json và logs/module-metadata.txt. Log Task 1 đầy đủ ở phụ lục A; log Task 2 đầy đủ và toàn bộ dmesg đi kèm archive.')
para('Trích dmesg thực tế sau kiểm thử và cleanup:')
selected=[x for x in dmesg.splitlines() if 'cleanup complete' in x]
code('\n'.join(selected))
para('Kết quả kiểm tra tự động:')
code(json.dumps(result,indent=2))
para('Tên archive: SE200778_PhamVoHoangNhan_Asgn01.tar.gz. PDF riêng nộp theo mục 4.4: SE200778_PhamVoHoangNhan_Asgn01_BaoCao.pdf. Trong bao_cao/ giữ SE200778_Asgn01_BaoCao.pdf theo cây thư mục mục 4.1; hai bản có cùng nội dung.')
para('Đề không quy định số ảnh chụp riêng. Minh chứng gồm log kiểm thử thực tế, metadata module và sơ đồ thiết kế. Deadline cần đối chiếu trên LMS; bộ tài liệu này chưa được tự động nộp lên hệ thống của trường.')
if not 8<=page<=20: raise RuntimeError(f'Invalid page count {page}')
c.save()
(OUT/'BaoCao.md').write_text('\n'.join(md))
(OUT/'report-metadata.json').write_text(json.dumps({'pages':page,'body_font_pt':12,
    'pdf':pdf.name,'source_run':(S/'logs/latest-run.txt').read_text().strip(),
    'sha256':hashlib.sha256(pdf.read_bytes()).hexdigest()},indent=2))
print('REPORT',pdf,'pages=',page)
