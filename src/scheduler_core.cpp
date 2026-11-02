// Pembungkus algoritma: satu-satunya file yang menyertakan jadwal_logic.h,
// sehingga data global dan fungsi non-inline di header itu hanya ada di satu
// translation unit. Algoritma di bawah dipanggil apa adanya, tidak diubah.

#include "scheduler_core.h"

#include "jadwal_logic.h"

#include <cctype>
#include <map>
#include <utility>

namespace {

MataKuliah toMk(const CourseData& c) {
    MataKuliah m;
    m.kode = c.kode;
    m.nama = c.nama;
    m.kelas = c.kelas;
    m.programStudi = c.programStudi;
    m.dosen = c.dosen;
    m.mhs = c.mhs;
    return m;
}

Ruang toRg(const RoomData& r) {
    Ruang g;
    g.nama = r.nama;
    g.kapasitas = r.kapasitas;
    g.vcr = r.vcr;
    return g;
}

std::vector<MataKuliah> toMkList(const std::vector<CourseData>& courses) {
    std::vector<MataKuliah> out;
    out.reserve(courses.size());
    for (const auto& c : courses) out.push_back(toMk(c));
    return out;
}

std::vector<Ruang> toRgList(const std::vector<RoomData>& rooms) {
    std::vector<Ruang> out;
    out.reserve(rooms.size());
    for (const auto& r : rooms) out.push_back(toRg(r));
    return out;
}

// Definisi bentrok:
// bentrok ruang = lebih dari satu mata kuliah pada (slot, ruang) yang sama;
// bentrok kelas = lebih dari satu mata kuliah pada (slot, prodi+kelas) yang
// sama;
// bentrok dosen = lebih dari satu mata kuliah dengan dosen yang sama pada
// slot yang sama (identitas dosen = ID, bukan teks nama).
// Ruang -1 (tidak ada ruang) tidak dihitung sebagai bentrok ruang.
// Selain menghitung, fungsi ini mencatat indeks mata kuliah lawan tiap
// bentrok, supaya UI bisa menjelaskan bentroknya dengan siapa.
void hitungBentrok(const std::vector<CourseData>& courses, ScheduleResult& out) {
    std::map<std::pair<int, int>, std::vector<int>> grupRuang;
    std::map<std::pair<int, std::string>, std::vector<int>> grupKelas;
    std::map<std::pair<int, int>, std::vector<int>> grupDosen;
    for (size_t i = 0; i < out.rows.size(); i++) {
        const ScheduleRow& row = out.rows[i];
        if (row.slot < 0) continue;
        const CourseData& c = courses.at(static_cast<size_t>(row.courseIndex));
        if (row.room >= 0) grupRuang[{row.slot, row.room}].push_back(static_cast<int>(i));
        grupKelas[{row.slot, c.programStudi + "_" + c.kelas}].push_back(static_cast<int>(i));
        if (c.dosenId >= 0)
            grupDosen[{row.slot, c.dosenId}].push_back(static_cast<int>(i));
    }
    auto hitungGrupBentrok = [](const auto& m) {
        int n = 0;
        for (const auto& kv : m)
            if (kv.second.size() > 1) n++;
        return n;
    };
    out.bentrokRuang = hitungGrupBentrok(grupRuang);
    out.bentrokKelas = hitungGrupBentrok(grupKelas);
    out.bentrokDosen = hitungGrupBentrok(grupDosen);
    for (size_t i = 0; i < out.rows.size(); i++) {
        ScheduleRow& row = out.rows[i];
        if (row.slot < 0) continue;
        const CourseData& c = courses.at(static_cast<size_t>(row.courseIndex));
        const auto itR = grupRuang.find({row.slot, row.room});
        if (row.room >= 0 && itR != grupRuang.end() && itR->second.size() > 1) {
            row.bentrokRuang = true;
            for (int j : itR->second) {
                if (j == static_cast<int>(i)) continue;
                row.konflikRuangDengan.push_back(out.rows[static_cast<size_t>(j)].courseIndex);
            }
        }
        const auto itK =
            grupKelas.find({row.slot, c.programStudi + "_" + c.kelas});
        if (itK != grupKelas.end() && itK->second.size() > 1) {
            row.bentrokKelas = true;
            for (int j : itK->second) {
                if (j == static_cast<int>(i)) continue;
                row.konflikKelasDengan.push_back(out.rows[static_cast<size_t>(j)].courseIndex);
            }
        }
        if (c.dosenId >= 0) {
            const auto itD = grupDosen.find({row.slot, c.dosenId});
            if (itD != grupDosen.end() && itD->second.size() > 1) {
                row.bentrokDosen = true;
                for (int j : itD->second) {
                    if (j == static_cast<int>(i)) continue;
                    row.konflikDosenDengan.push_back(out.rows[static_cast<size_t>(j)].courseIndex);
                }
            }
        }
    }
}

}  // namespace

std::vector<Lecturer> sampleLecturers() {
    std::vector<Lecturer> out;
    std::map<std::string, int> idByName;
    int nextId = 1;
    for (const auto& m : ::mk) {
        if (idByName.count(m.dosen) == 0) {
            idByName[m.dosen] = nextId;
            out.push_back({nextId, m.dosen});
            nextId++;
        }
    }
    return out;
}

std::vector<CourseData> sampleCourses() {
    const std::vector<Lecturer> dosens = sampleLecturers();
    std::map<std::string, int> idByName;
    for (const auto& d : dosens) idByName[d.nama] = d.id;
    std::vector<CourseData> out;
    out.reserve(::mk.size());
    for (const auto& m : ::mk)
        out.push_back({m.kode, m.nama, m.kelas, m.programStudi, m.dosen,
                       idByName[m.dosen], m.mhs});
    return out;
}

std::vector<RoomData> sampleRooms() {
    std::vector<RoomData> out;
    out.reserve(::rg.size());
    for (const auto& r : ::rg) {
        RoomData d;
        d.nama = r.nama;
        d.kapasitas = r.kapasitas;
        // Ruangan VCR dikenali dari namanya (data asli: VCR-FIK-KB-1).
        const std::string lower = [&] {
            std::string s = r.nama;
            for (auto& ch : s)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return s;
        }();
        d.vcr = (lower.find("vcr") != std::string::npos);
        out.push_back(d);
    }
    return out;
}

ScheduleResult computeWelshPowell(const std::vector<CourseData>& courses,
                                  const std::vector<RoomData>& rooms) {
    ScheduleResult out;
    const int n = static_cast<int>(courses.size());
    out.rows.assign(static_cast<size_t>(n), ScheduleRow{});
    for (int i = 0; i < n; i++)
        out.rows[static_cast<size_t>(i)].courseIndex = i;
    if (n == 0) {
        out.feasible = true;
        out.terjadwalSemua = true;
        out.valid = true;
        return out;
    }
    const std::vector<MataKuliah> mkIn = toMkList(courses);
    const std::vector<Ruang> rgIn = toRgList(rooms);
    const HasilWP r = welshPowellOriginal(mkIn);
    out.jumlahSlot = r.jumlahWarna;
    out.feasible = true;
    out.terjadwalSemua = true;
    const int R = static_cast<int>(rgIn.size());
    // Pembagian ruang round-robin per warna, seperti menuWelshPowell() di
    // file asli, tetapi dengan aturan VCR: mata kuliah 40 mahasiswa atau
    // lebih hanya digilir di ruangan VCR, sisanya hanya di ruangan biasa.
    // Kalau satu slot isinya melebihi jumlah ruangan yang eligible, giliran
    // muter balik dan bentrok ruang tetap terjadi (jadwal awal memang kasar).
    std::vector<int> vcrIdx, regIdx;
    for (int ri = 0; ri < R; ri++)
        (rgIn[static_cast<size_t>(ri)].vcr ? vcrIdx : regIdx).push_back(ri);
    std::map<int, int> counterVcr, counterReg;
    out.tanpaRuang = 0;
    for (int i = 0; i < n; i++) {
        const int slot = r.warna[static_cast<size_t>(i)];
        ScheduleRow& row = out.rows[static_cast<size_t>(i)];
        row.slot = slot;
        const bool butuhVcr = courses[static_cast<size_t>(i)].mhs >= 40;
        if (butuhVcr) {
            if (vcrIdx.empty()) {
                row.room = -1;
                row.tanpaRuang = true;
            } else {
                row.room = vcrIdx[static_cast<size_t>(counterVcr[slot]++) %
                                  vcrIdx.size()];
            }
        } else {
            if (regIdx.empty()) {
                row.room = -1;
                row.tanpaRuang = true;
            } else {
                row.room = regIdx[static_cast<size_t>(counterReg[slot]++) %
                                  regIdx.size()];
            }
        }
        if (row.tanpaRuang) out.tanpaRuang++;
    }
    hitungBentrok(courses, out);
    out.valid = (out.bentrokRuang == 0 && out.bentrokKelas == 0 &&
                 out.bentrokDosen == 0 && out.tanpaRuang == 0);
    return out;
}

ScheduleResult computeDinic(const std::vector<CourseData>& courses,
                            const std::vector<RoomData>& rooms) {
    ScheduleResult out;
    const int n = static_cast<int>(courses.size());
    out.rows.assign(static_cast<size_t>(n), ScheduleRow{});
    for (int i = 0; i < n; i++)
        out.rows[static_cast<size_t>(i)].courseIndex = i;
    if (n == 0) {
        out.feasible = true;
        out.terjadwalSemua = true;
        out.valid = true;
        return out;
    }
    if (rooms.empty()) {
        out.feasible = false;
        out.terjadwalSemua = false;
        return out;
    }
    const std::vector<MataKuliah> mkIn = toMkList(courses);
    const std::vector<Ruang> rgIn = toRgList(rooms);
    const HasilEfisien h = selesaikan(mkIn, rgIn);
    out.batasBawah = h.batasBawah;
    out.maksGrup = h.maksGrup;
    if (h.jumlahSlot == 0) {
        // Tidak ada alokasi yang muat dalam 20 slot (Senin-Jumat x 4 jam).
        out.feasible = false;
        out.terjadwalSemua = false;
        return out;
    }
    out.feasible = true;
    out.terjadwalSemua = true;
    out.jumlahSlot = h.jumlahSlot;
    for (int i = 0; i < n; i++) {
        ScheduleRow& row = out.rows[static_cast<size_t>(i)];
        row.slot = h.slot[static_cast<size_t>(i)];
        row.room = h.ruang[static_cast<size_t>(i)];
        row.tanpaRuang = (row.room < 0);
        if (row.tanpaRuang) out.tanpaRuang++;
    }
    hitungBentrok(courses, out);
    out.valid = (out.bentrokRuang == 0 && out.bentrokKelas == 0 &&
                 out.bentrokDosen == 0 && out.tanpaRuang == 0);
    return out;
}

std::string slotLabel(int slot) { return labelSlot(slot); }

std::string prodiName(const std::string& kode) { return getNamaProdi(kode); }
