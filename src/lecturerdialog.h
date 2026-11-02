#pragma once

#include "scheduler_core.h"

#include <QDialog>
#include <string>

class QLineEdit;

// Dialog tambah/ubah satu dosen (data master untuk select dosen).
class LecturerDialog : public QDialog {
    Q_OBJECT
public:
    // Mengembalikan true jika pengguna menekan OK dan data valid.
    // `data` dibaca sebagai nilai awal dan ditulis kembali saat OK.
    static bool editLecturer(QWidget* parent, Lecturer& data, bool isNew);

private:
    LecturerDialog(QWidget* parent, const Lecturer& data, bool isNew);
    void accept() override;

    QLineEdit* m_nama = nullptr;
    Lecturer m_data;
};
