#pragma once

#include "scheduler_core.h"

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QSpinBox;

// Dialog tambah/ubah satu ruangan.
class RoomDialog : public QDialog {
    Q_OBJECT
public:
    // Mengembalikan true jika pengguna menekan OK dan data valid.
    // `data` dibaca sebagai nilai awal dan ditulis kembali saat OK.
    static bool editRoom(QWidget* parent, RoomData& data, bool isNew);

private:
    RoomDialog(QWidget* parent, const RoomData& data, bool isNew);
    void accept() override;

    QLineEdit* m_nama = nullptr;
    QSpinBox* m_kapasitas = nullptr;
    QCheckBox* m_vcr = nullptr;
    RoomData m_data;
};
