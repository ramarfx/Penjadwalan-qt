#include "lecturerdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

LecturerDialog::LecturerDialog(QWidget* parent, const Lecturer& data, bool isNew)
    : QDialog(parent), m_data(data) {
    setWindowTitle(isNew ? "Tambah Dosen" : "Ubah Dosen");
    setModal(true);

    m_nama = new QLineEdit(this);
    m_nama->setText(QString::fromStdString(data.nama));
    m_nama->setPlaceholderText("mis. Dr. Nurjanah");

    auto* form = new QFormLayout;
    form->addRow("Nama dosen:", m_nama);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &LecturerDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &LecturerDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    m_nama->setFocus();
}

void LecturerDialog::accept() {
    if (m_nama->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Data belum lengkap",
                             "Nama dosen harus diisi.");
        return;
    }
    m_data.nama = m_nama->text().trimmed().toStdString();
    QDialog::accept();
}

bool LecturerDialog::editLecturer(QWidget* parent, Lecturer& data, bool isNew) {
    LecturerDialog dlg(parent, data, isNew);
    if (dlg.exec() == QDialog::Accepted) {
        data = dlg.m_data;
        return true;
    }
    return false;
}
