# Penjadwalan Mata Kuliah (Qt)

Aplikasi desktop penjadwalan mata kuliah berbasis Qt 6 Widgets. Bukan
benchmark: aplikasi ini memang dibuat untuk dipakai, dengan input data,
penjadwalan otomatis, dan ekspor hasil.

## Cara pakai

1. Isi daftar **Mata Kuliah** (kode, nama, kelas, program studi, dosen,
   jumlah mahasiswa) lewat tombol Tambah, atau klik dua kali baris untuk
   mengubahnya. Dosen dipilih dari daftar **Dosen** (select), bukan diketik
   bebas, supaya tidak ada variasi penulisan nama.
2. Isi daftar **Dosen** (nama) dan **Ruangan** (nama, kapasitas, centang
   VCR bila ruangan itu VCR) dengan cara yang sama.
3. Klik **Jadwalkan Otomatis**. Aplikasi menghitung jadwal awal dengan
   Welsh-Powell, lalu mengoptimasinya dengan aliran maksimum Dinic, dan
   menampilkan hasilnya di tab **Hasil Jadwal**.
4. Di tab hasil, ganti tampilan antara Jadwal Optimasi (Dinic) dan Jadwal
   Awal (Welsh-Powell), saring per program studi, atau cari berdasarkan kode,
   nama, dan dosen. Baris yang bentrok ditandai merah, baris tanpa ruang yang
   muat ditandai kuning. Kolom Status menyebut bentroknya dengan mata kuliah
   siapa, dan kotak "Penjelasan baris terpilih" di bawah tabel menjelaskan
   dengan kalimat biasa kenapa baris yang dipilih aman atau bentrok.
   Centang "Hanya yang bermasalah" untuk melihat hanya baris yang bentrok
   atau tanpa ruang.
5. Data bisa disimpan dan dibuka kembali sebagai JSON (menu File), dan hasil
   jadwal bisa diekspor ke CSV (menu File > Ekspor Hasil ke CSV).

Aplikasi dibuka dengan data contoh dari file asli (184 mata kuliah, 13 ruang),
supaya langsung bisa dicoba. Tombol **Muat Data Contoh** mengembalikan data
contoh itu kapan saja; **Hapus Semua** mengosongkan semuanya.

## Algoritma dan aturan

Algoritma Welsh-Powell dan Dinic dipakai dari `efisiensibentrok.cpp`
(`gabungan_jadwal_final.cpp`). Struktur algoritmanya dipertahankan; dua
aturan dari spek tugas ditambahkan dan ditandai di `src/jadwal_logic.h`:

1. **Bentrok dosen**: dua mata kuliah dengan dosen yang sama tidak boleh
   satu slot. Welsh-Powell menerimanya sebagai sisi graf (pewarnaan tidak
   pernah menaruh mata kuliah sedosen di satu warna), dan deteksi bentrok
   menghitungnya sebagai jenis bentrok tersendiri.
2. **Aturan VCR**: mata kuliah 40 mahasiswa atau lebih hanya menempati
   ruangan VCR. Pembagian ruang Dinic (best-fit) dan pembagian ruang
   round-robin Welsh-Powell sama-sama mematuhi aturan ini.

Dosen disimpan sebagai data master ber-ID (`src/scheduler_core.h`,
`Lecturer`); mata kuliah merujuk dosen lewat ID, dan nama yang tampil
adalah nama kanonis dari daftar dosen. File JSON versi 2 menyimpan daftar
dosen, ID dosen tiap mata kuliah, dan flag VCR tiap ruangan; file JSON lama
(tanpa daftar dosen/flag VCR) tetap bisa dibuka, dosen diturunkan dari
teks nama dan VCR dikenali dari nama ruangan.

Slot waktu mengikuti file asli: Senin-Jumat, 4 jam kuliah per hari
(07.10-08.50, 09.40-11.20, 13.00-14.40, 15.30-17.10), total 20 slot.

## Build (Linux, Qt 6)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/penjadwalan-qt
```

Butuh Qt 6 Widgets + CMake + compiler C++17 (g++ atau clang). Di Arch/CachyOS:
`qt6-base` dan `cmake` sudah cukup.

## Catatan pengujian

Variabel lingkungan `PENJADWALAN_QT_AUTOTEST=1` hanya dipakai untuk verifikasi
headless: aplikasi langsung menjadwalkan dan membuka tab hasil saat start.
`PENJADWALAN_QT_AUTOTEST_WP=1` menampilkan hasil Welsh-Powell, bukan Dinic.

## Windows (.exe)

Versi ini dibangun dan diuji di Linux. Build Windows (.exe) butuh Qt yang
dibangun untuk Windows (MinGW/MSVC) dan belum dicoba di sini; bila dibutuhkan,
jalur yang paling aman adalah membangun dari source ini di mesin Windows yang
sudah terpasang Qt 6 (Qt Creator atau `cmake` + MinGW), karena `CMakeLists.txt`
tidak mengandung hal spesifik Linux.
