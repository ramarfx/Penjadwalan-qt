#pragma once

// Model data + pembungkus algoritma untuk aplikasi Qt.
// Algoritma (Welsh-Powell dan Dinic) TIDAK ditulis ulang di sini: implementasi
// verbatim-nya ada di jadwal_logic.h dan hanya dipakai lewat file ini.
// Lapisan ini hanya mengubah data UI (CourseData/RoomData) menjadi tipe yang
// dipakai algoritma, lalu mengubah hasilnya kembali untuk ditampilkan.

#include <string>
#include <vector>

// Dosen sebagai data master: identitasnya ID, bukan teks nama. Nama yang
// diketik bebas rawan variasi ("Dr. Nurjanah" vs "Nurjanah") dan merusak
// deteksi bentrok dosen yang membandingkan identitas. Mata kuliah hanya
// boleh memilih dosen dari daftar ini (select), dan `dosen` di CourseData
// adalah salinan nama kanonis untuk tampilan/algoritma.
struct Lecturer {
    int id = -1;
    std::string nama;
};

struct CourseData {
    std::string kode;
    std::string nama;
    std::string kelas;
    std::string programStudi;
    std::string dosen;
    int dosenId = -1;
    int mhs = 35;
};

struct RoomData {
    std::string nama;
    int kapasitas = 40;
    // Ruangan VCR: satu-satunya jenis ruangan untuk mata kuliah dengan
    // 40 mahasiswa atau lebih (aturan dari awal). Data contoh menandai
    // VCR-FIK-KB-1 sebagai VCR.
    bool vcr = false;
};

struct ScheduleRow {
    int courseIndex = -1;
    int slot = -1;  // -1 = tidak terjadwal
    int room = -1;  // -1 = tidak ada ruang yang muat
    bool bentrokRuang = false;
    bool bentrokKelas = false;
    bool bentrokDosen = false;
    bool tanpaRuang = false;
    // Indeks mata kuliah lain yang bentrok dengan baris ini, supaya UI bisa
    // menjelaskan bentroknya dengan siapa (bukan sekadar "bentrok").
    std::vector<int> konflikRuangDengan;
    std::vector<int> konflikKelasDengan;
    std::vector<int> konflikDosenDengan;
};

struct ScheduleResult {
    std::vector<ScheduleRow> rows;
    int jumlahSlot = 0;
    int batasBawah = 0;
    int maksGrup = 0;
    int bentrokRuang = 0;
    int bentrokKelas = 0;
    int bentrokDosen = 0;
    int tanpaRuang = 0;
    bool terjadwalSemua = false;
    bool feasible = false;
    bool valid = false;
};

// Data contoh bawaan dari file asli (184 mata kuliah, 13 ruang).
// Daftar dosen diturunkan dari data contoh (nama unik, urutan kemunculan
// pertama) supaya ID-nya deterministik dan sama dengan sampleCourses().
std::vector<Lecturer> sampleLecturers();
std::vector<CourseData> sampleCourses();
std::vector<RoomData> sampleRooms();

// Jadwal awal Welsh-Powell: slot = warna, ruang = round-robin per warna,
// persis seperti menuWelshPowell() di file asli (termasuk bentroknya).
ScheduleResult computeWelshPowell(const std::vector<CourseData>& courses,
                                  const std::vector<RoomData>& rooms);

// Jadwal optimasi Dinic: memanggil selesaikan() dari file asli apa adanya.
ScheduleResult computeDinic(const std::vector<CourseData>& courses,
                            const std::vector<RoomData>& rooms);

// Label slot dan nama prodi, mengikuti helper file asli.
std::string slotLabel(int slot);
std::string prodiName(const std::string& kode);
