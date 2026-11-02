#pragma once

#include "scheduler_core.h"

#include <QDialog>
#include <functional>
#include <string>
#include <vector>

class QComboBox;
class QLineEdit;
class QSpinBox;

// Dialog tambah/ubah satu mata kuliah. Dosen dipilih dari daftar dosen
// (select berdasarkan ID), bukan diketik bebas, supaya tidak ada variasi
// penulisan nama yang merusak deteksi bentrok dosen.
class CourseDialog : public QDialog {
    Q_OBJECT
public:
    // Mengembalikan true jika pengguna menekan OK dan data valid.
    // `data` dibaca sebagai nilai awal dan ditulis kembali saat OK.
    // `addLecturer` dipanggil saat pengguna menambah dosen baru lewat
    // tombol + dan mengembalikan ID dosen yang baru dibuat.
    static bool editCourse(QWidget* parent, CourseData& data, bool isNew,
                           const std::vector<std::string>& prodiList,
                           const std::vector<Lecturer>& lecturers,
                           const std::function<int(const std::string&)>& addLecturer);

private:
    CourseDialog(QWidget* parent, const CourseData& data, bool isNew,
                 const std::vector<std::string>& prodiList,
                 const std::vector<Lecturer>& lecturers,
                 const std::function<int(const std::string&)>& addLecturer);
    void accept() override;
    void onAddLecturerClicked();
    void refillLecturerCombo(int selectId);

    QLineEdit* m_kode = nullptr;
    QLineEdit* m_nama = nullptr;
    QLineEdit* m_kelas = nullptr;
    QComboBox* m_prodi = nullptr;
    QComboBox* m_dosen = nullptr;
    QSpinBox* m_mhs = nullptr;
    CourseData m_data;
    std::vector<Lecturer> m_lecturers;
    std::function<int(const std::string&)> m_addLecturer;
};
