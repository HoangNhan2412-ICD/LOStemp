# ASM1 — checklist kiểm chứng

Nguồn: ASSIGNMENT01_EmbeddedLinux.docx, mục III–V. Các kết quả thực hành nằm trong practice/; file mẫu được giữ nguyên.

- [x] Task 1: 4 ioctl có header riêng, major 241; PASS trong logs/test_asgn1_output.txt. Mutex và uaccess đã rà soát. Phân bổ rubric: RESET 10, STATS 12, MODE 10, VERSION 8; không tự chấm điểm.
- [x] Task 2: misc/atomic, read/write/offset/procfs; PASS trong logs/test_counter_output.txt. Metadata Phạm Võ Hoàng Nhân — SE200778. Rubric 8/8/7/7; không tự chấm điểm.
- [x] Task 3: PDF trang 2–7 và phụ lục: sơ đồ chung, bảng so sánh, phân tích, log thật và lỗi thực tế.
- [x] Task 4: PDF trang 8–9 trả lời 4 câu, có đối chiếu mã nguồn kernel và tài liệu.
- [x] Build và clean: logs/build-clean-verification.txt; ARM và vermagic: logs/module-metadata.txt; nạp/test/gỡ thật: logs/result.json.
- [x] PDF 15 trang, nội dung 12pt, đánh số; archive có source, .ko, test, log, báo cáo, manifest.
- [x] SHA-256 file gốc không đổi qua test; git diff và cached diff rỗng; không commit/push/reset.

Sinh viên: Phạm Võ Hoàng Nhân, SE200778. MODULE_AUTHOR, version và báo cáo đã đồng bộ với bản build/test cuối.
Đối chiếu quy định nộp bài tại mục IV–V của đề trước khi nộp.

## Phần chưa thể tự xác nhận

- [ ] Người học kiểm tra quy định tác giả và nguồn tham khảo tại mục 4.3 và V.
- [ ] Deadline và thao tác nộp LMS: đề đang để giảng viên điền, không có yêu cầu tự nộp.
- Không tuyên bố 100/100 hoặc hoàn thành quy định học thuật chỉ dựa vào test PASS.
