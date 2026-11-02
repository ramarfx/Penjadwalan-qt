#include "roomdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

RoomDialog::RoomDialog(QWidget* parent, const RoomData& data, bool isNew)
    : QDialog(parent), m_data(data) {
    setWindowTitle(isNew ? "Tambah Ruangan" : "Ubah Ruangan");
    setModal(true);

    m_nama = new QLineEdit(this);
    m_kapasitas = new QSpinBox(this);
    m_kapasitas->setRange(1, 500);
    m_vcr = new QCheckBox(
        "Ruangan VCR (untuk mata kuliah 40 mahasiswa atau lebih)", this);

    m_nama->setText(QString::fromStdString(data.nama));
    m_kapasitas->setValue(data.kapasitas);
    m_vcr->setChecked(data.vcr);
    m_nama->setPlaceholderText("mis. FIK-201");

    auto* form = new QFormLayout;
    form->addRow("Nama ruangan:", m_nama);
    form->addRow("Kapasitas:", m_kapasitas);
    form->addRow("", m_vcr);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &RoomDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &RoomDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    m_nama->setFocus();
}

void RoomDialog::accept() {
    if (m_nama->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Data belum lengkap",
                             "Nama ruangan harus diisi.");
        return;
    }
    m_data.nama = m_nama->text().trimmed().toStdString();
    m_data.kapasitas = m_kapasitas->value();
    m_data.vcr = m_vcr->isChecked();
    QDialog::accept();
}

bool RoomDialog::editRoom(QWidget* parent, RoomData& data, bool isNew) {
    RoomDialog dlg(parent, data, isNew);
    if (dlg.exec() == QDialog::Accepted) {
        data = dlg.m_data;
        return true;
    }
    return false;
}
