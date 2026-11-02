#include "coursedialog.h"

#include "lecturerdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

CourseDialog::CourseDialog(QWidget* parent, const CourseData& data, bool isNew,
                           const std::vector<std::string>& prodiList,
                           const std::vector<Lecturer>& lecturers,
                           const std::function<int(const std::string&)>& addLecturer)
    : QDialog(parent), m_data(data), m_lecturers(lecturers), m_addLecturer(addLecturer) {
    setWindowTitle(isNew ? "Tambah Mata Kuliah" : "Ubah Mata Kuliah");
    setModal(true);

    m_kode = new QLineEdit(this);
    m_nama = new QLineEdit(this);
    m_kelas = new QLineEdit(this);
    m_prodi = new QComboBox(this);
    m_prodi->setEditable(true);
    for (const auto& p : prodiList) m_prodi->addItem(QString::fromStdString(p));
    m_dosen = new QComboBox(this);
    auto* addDosenBtn = new QPushButton("+", this);
    addDosenBtn->setToolTip("Tambah dosen baru ke daftar.");
    addDosenBtn->setFixedWidth(32);
    connect(addDosenBtn, &QPushButton::clicked, this,
            &CourseDialog::onAddLecturerClicked);
    auto* dosenRow = new QHBoxLayout;
    dosenRow->addWidget(m_dosen, 1);
    dosenRow->addWidget(addDosenBtn);
    m_mhs = new QSpinBox(this);
    m_mhs->setRange(1, 500);

    m_kode->setText(QString::fromStdString(data.kode));
    m_nama->setText(QString::fromStdString(data.nama));
    m_kelas->setText(QString::fromStdString(data.kelas));
    m_prodi->setCurrentText(QString::fromStdString(data.programStudi));
    refillLecturerCombo(data.dosenId);
    if (m_dosen->currentIndex() < 0 && !data.dosen.empty()) {
        // Data lama tanpa ID: cocokkan dengan nama bila masih ada.
        const int byName = m_dosen->findText(QString::fromStdString(data.dosen));
        if (byName >= 0) m_dosen->setCurrentIndex(byName);
    }
    m_mhs->setValue(data.mhs);

    m_kode->setPlaceholderText("mis. INF301A");
    m_nama->setPlaceholderText("mis. Metode Penelitian");
    m_kelas->setPlaceholderText("mis. A");

    auto* form = new QFormLayout;
    form->addRow("Kode:", m_kode);
    form->addRow("Mata kuliah:", m_nama);
    form->addRow("Kelas:", m_kelas);
    form->addRow("Program studi:", m_prodi);
    form->addRow("Dosen:", dosenRow);
    form->addRow("Jumlah mahasiswa:", m_mhs);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &CourseDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &CourseDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    m_kode->setFocus();
}

void CourseDialog::refillLecturerCombo(int selectId) {
    m_dosen->blockSignals(true);
    m_dosen->clear();
    for (const auto& d : m_lecturers)
        m_dosen->addItem(QString::fromStdString(d.nama), d.id);
    const int idx = (selectId >= 0) ? m_dosen->findData(selectId) : -1;
    m_dosen->setCurrentIndex(idx);
    m_dosen->blockSignals(false);
}

void CourseDialog::onAddLecturerClicked() {
    Lecturer fresh;
    if (!LecturerDialog::editLecturer(this, fresh, true)) return;
    if (!m_addLecturer) return;
    const int newId = m_addLecturer(fresh.nama);
    // Daftar dosen dipegang MainWindow; sinkronkan salinan lokal dari hasil
    // callback (nama mungkin sudah ada dan digabung ke ID lama).
    bool found = false;
    for (const auto& d : m_lecturers) {
        if (d.id == newId) {
            found = true;
            break;
        }
    }
    if (!found) m_lecturers.push_back({newId, fresh.nama});
    refillLecturerCombo(newId);
}

void CourseDialog::accept() {
    if (m_kode->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Data belum lengkap",
                             "Kode mata kuliah harus diisi.");
        return;
    }
    if (m_nama->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Data belum lengkap",
                             "Nama mata kuliah harus diisi.");
        return;
    }
    if (m_kelas->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Data belum lengkap", "Kelas harus diisi.");
        return;
    }
    if (m_prodi->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Data belum lengkap",
                             "Program studi harus diisi.");
        return;
    }
    if (m_dosen->currentIndex() < 0) {
        QMessageBox::warning(this, "Data belum lengkap",
                             "Pilih dosen dari daftar. Kalau dosennya belum "
                             "ada, tambah dulu lewat tombol + di samping "
                             "kolom Dosen.");
        return;
    }
    m_data.kode = m_kode->text().trimmed().toStdString();
    m_data.nama = m_nama->text().trimmed().toStdString();
    m_data.kelas = m_kelas->text().trimmed().toStdString();
    m_data.programStudi = m_prodi->currentText().trimmed().toStdString();
    m_data.dosenId = m_dosen->currentData().toInt();
    m_data.dosen = m_dosen->currentText().toStdString();
    m_data.mhs = m_mhs->value();
    QDialog::accept();
}

bool CourseDialog::editCourse(QWidget* parent, CourseData& data, bool isNew,
                              const std::vector<std::string>& prodiList,
                              const std::vector<Lecturer>& lecturers,
                              const std::function<int(const std::string&)>& addLecturer) {
    CourseDialog dlg(parent, data, isNew, prodiList, lecturers, addLecturer);
    if (dlg.exec() == QDialog::Accepted) {
        data = dlg.m_data;
        return true;
    }
    return false;
}
