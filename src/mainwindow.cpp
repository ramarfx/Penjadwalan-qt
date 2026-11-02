#include "mainwindow.h"

#include "coursedialog.h"
#include "lecturerdialog.h"
#include "roomdialog.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <utility>

namespace {

constexpr int kIndexRole = Qt::UserRole + 1;
const QColor kConflictBg(254, 202, 202);
const QColor kNoRoomBg(253, 230, 138);

QStandardItem* makeItem(const QString& text, const QVariant& sortValue, int index) {
    auto* item = new QStandardItem(text);
    item->setEditable(false);
    item->setData(sortValue, Qt::UserRole);
    item->setData(index, kIndexRole);
    return item;
}

QString csvField(const QString& value) {
    if (value.contains(',') || value.contains('"') || value.contains('\n')) {
        QString escaped = value;
        escaped.replace("\"", "\"\"");
        return "\"" + escaped + "\"";
    }
    return value;
}

QString joinedNames(const std::vector<int>& indices,
                    const std::vector<CourseData>& courses) {
    QStringList names;
    for (int idx : indices)
        names << QString::fromStdString(
            courses.at(static_cast<size_t>(idx)).kode);
    if (names.size() > 2)
        return names.mid(0, 2).join(", ") + QString(" +%1 lainnya").arg(names.size() - 2);
    return names.join(", ");
}

// Status singkat untuk kolom Status: menyebut bentroknya dengan siapa,
// bukan sekadar kata "bentrok".
QString statusText(const ScheduleRow& row,
                   const std::vector<CourseData>& courses) {
    if (row.slot < 0) return "Tidak terjadwal";
    QStringList parts;
    if (row.bentrokRuang)
        parts << "Bentrok ruang: " + joinedNames(row.konflikRuangDengan, courses);
    if (row.bentrokKelas)
        parts << "Bentrok kelas: " + joinedNames(row.konflikKelasDengan, courses);
    if (row.bentrokDosen)
        parts << "Bentrok dosen: " + joinedNames(row.konflikDosenDengan, courses);
    if (row.tanpaRuang) {
        const CourseData& c = courses.at(static_cast<size_t>(row.courseIndex));
        parts << (c.mhs >= 40 ? "Tanpa ruang VCR" : "Tanpa ruang muat");
    }
    if (parts.isEmpty()) return "OK";
    return parts.join("; ");
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    // Aplikasi dibuka dengan data contoh dari file asli, supaya langsung
    // bisa dicoba. Pengguna bebas menghapus atau mengubahnya.
    m_courses = sampleCourses();
    m_dosens = sampleLecturers();
    m_nextDosenId = 1;
    for (const auto& d : m_dosens)
        m_nextDosenId = std::max(m_nextDosenId, d.id + 1);
    m_rooms = sampleRooms();
    refreshCourseTable();
    refreshDosenTable();
    refreshRoomTable();
    refreshProdiFilter();
    refreshResultTable();
    refreshSummary();
    updateCounts();
    setDirty(false);
    statusBar()->showMessage(
        "Data contoh dimuat. Klik Jadwalkan Otomatis untuk membuat jadwal.", 8000);

    // Kait pengujian headless saja (tidak dipakai saat pemakaian normal):
    // PENJADWALAN_QT_AUTOTEST=1 -> jadwalkan lalu tampilkan tab hasil.
    // PENJADWALAN_QT_AUTOTEST_WP=1 -> tampilkan hasil Welsh-Powell.
    // PENJADWALAN_QT_AUTOTEST_DOSEN=1 -> tampilkan tab Dosen.
    if (qEnvironmentVariableIsSet("PENJADWALAN_QT_AUTOTEST")) {
        QTimer::singleShot(0, this, [this] {
            onSchedule();
            if (qEnvironmentVariableIsSet("PENJADWALAN_QT_AUTOTEST_WP"))
                m_viewCombo->setCurrentIndex(1);
            if (m_resultModel->rowCount() > 0) m_resultView->selectRow(0);
        });
    }
    if (qEnvironmentVariableIsSet("PENJADWALAN_QT_AUTOTEST_DOSEN")) {
        QTimer::singleShot(0, this, [this] { m_tabs->setCurrentIndex(1); });
    }
}

void MainWindow::setupUi() {
    setWindowTitle("Penjadwalan Mata Kuliah");
    resize(1100, 700);

    auto* fileMenu = menuBar()->addMenu("&File");
    QAction* actNew = fileMenu->addAction("&Baru", this, &MainWindow::onNewFile);
    actNew->setShortcut(QKeySequence::New);
    QAction* actOpen = fileMenu->addAction("&Buka...", this, &MainWindow::onOpenFile);
    actOpen->setShortcut(QKeySequence::Open);
    QAction* actSave = fileMenu->addAction("&Simpan", this, &MainWindow::onSaveFile);
    actSave->setShortcut(QKeySequence::Save);
    fileMenu->addAction("Simpan &Sebagai...", this, &MainWindow::onSaveFileAs);
    fileMenu->addSeparator();
    fileMenu->addAction("&Ekspor Hasil ke CSV...", this, &MainWindow::onExportCsv);
    fileMenu->addSeparator();
    QAction* actQuit = fileMenu->addAction("&Keluar", this, &QWidget::close);
    actQuit->setShortcut(QKeySequence::Quit);

    auto* helpMenu = menuBar()->addMenu("&Bantuan");
    helpMenu->addAction("&Tentang", this, &MainWindow::onAbout);

    auto* toolbar = addToolBar("Utama");
    toolbar->setMovable(false);
    auto* scheduleBtn = new QPushButton("Jadwalkan Otomatis", this);
    scheduleBtn->setStyleSheet(
        "QPushButton { background-color: #1D4ED8; color: white; font-weight: bold;"
        " padding: 8px 20px; font-size: 14px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1E40AF; }");
    scheduleBtn->setToolTip(
        "Hitung jadwal dari data saat ini (Welsh-Powell lalu optimasi Dinic).");
    connect(scheduleBtn, &QPushButton::clicked, this, &MainWindow::onSchedule);
    toolbar->addWidget(scheduleBtn);
    toolbar->addSeparator();
    toolbar->addAction("Muat Data Contoh", this, &MainWindow::onLoadSample);
    toolbar->addAction("Hapus Semua", this, &MainWindow::onClearAll);

    m_tabs = new QTabWidget(this);
    setCentralWidget(m_tabs);

    // ---- Tab Mata Kuliah ----
    auto* coursePage = new QWidget(this);
    auto* courseLayout = new QVBoxLayout(coursePage);
    auto* courseBtnRow = new QHBoxLayout;
    auto* addCourseBtn = new QPushButton("Tambah", coursePage);
    auto* editCourseBtn = new QPushButton("Ubah", coursePage);
    auto* delCourseBtn = new QPushButton("Hapus", coursePage);
    courseBtnRow->addWidget(addCourseBtn);
    courseBtnRow->addWidget(editCourseBtn);
    courseBtnRow->addWidget(delCourseBtn);
    courseBtnRow->addStretch();
    courseLayout->addLayout(courseBtnRow);
    m_courseModel = new QStandardItemModel(this);
    m_courseModel->setHorizontalHeaderLabels(
        {"Kode", "Mata Kuliah", "Kelas", "Program Studi", "Dosen", "Jumlah Mhs"});
    m_courseModel->setSortRole(Qt::UserRole);
    m_courseView = new QTableView(this);
    m_courseView->setModel(m_courseModel);
    m_courseView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_courseView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_courseView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_courseView->setSortingEnabled(true);
    m_courseView->setAlternatingRowColors(true);
    m_courseView->verticalHeader()->setVisible(false);
    m_courseView->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    m_courseView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_courseView->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    m_courseView->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    m_courseView->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::ResizeToContents);
    m_courseView->horizontalHeader()->setSectionResizeMode(
        5, QHeaderView::ResizeToContents);
    courseLayout->addWidget(m_courseView);
    m_tabs->addTab(coursePage, "Mata Kuliah");
    connect(addCourseBtn, &QPushButton::clicked, this, &MainWindow::onAddCourse);
    connect(editCourseBtn, &QPushButton::clicked, this, &MainWindow::onEditCourse);
    connect(delCourseBtn, &QPushButton::clicked, this, &MainWindow::onDeleteCourse);
    connect(m_courseView, &QTableView::doubleClicked, this,
            &MainWindow::onCourseDoubleClicked);
    auto* delCourseShortcut = new QShortcut(QKeySequence::Delete, m_courseView);
    connect(delCourseShortcut, &QShortcut::activated, this,
            &MainWindow::onDeleteCourse);

    // ---- Tab Dosen ----
    auto* dosenPage = new QWidget(this);
    auto* dosenLayout = new QVBoxLayout(dosenPage);
    auto* dosenBtnRow = new QHBoxLayout;
    auto* addDosenBtn = new QPushButton("Tambah", dosenPage);
    auto* editDosenBtn = new QPushButton("Ubah", dosenPage);
    auto* delDosenBtn = new QPushButton("Hapus", dosenPage);
    dosenBtnRow->addWidget(addDosenBtn);
    dosenBtnRow->addWidget(editDosenBtn);
    dosenBtnRow->addWidget(delDosenBtn);
    dosenBtnRow->addStretch();
    dosenLayout->addLayout(dosenBtnRow);
    m_dosenModel = new QStandardItemModel(this);
    m_dosenModel->setHorizontalHeaderLabels(
        {"Nama Dosen", "Jumlah Mata Kuliah"});
    m_dosenModel->setSortRole(Qt::UserRole);
    m_dosenView = new QTableView(this);
    m_dosenView->setModel(m_dosenModel);
    m_dosenView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dosenView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dosenView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dosenView->setSortingEnabled(true);
    m_dosenView->setAlternatingRowColors(true);
    m_dosenView->verticalHeader()->setVisible(false);
    m_dosenView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_dosenView->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    dosenLayout->addWidget(m_dosenView);
    m_tabs->addTab(dosenPage, "Dosen");
    connect(addDosenBtn, &QPushButton::clicked, this, &MainWindow::onAddDosen);
    connect(editDosenBtn, &QPushButton::clicked, this, &MainWindow::onEditDosen);
    connect(delDosenBtn, &QPushButton::clicked, this, &MainWindow::onDeleteDosen);
    connect(m_dosenView, &QTableView::doubleClicked, this,
            &MainWindow::onDosenDoubleClicked);
    auto* delDosenShortcut = new QShortcut(QKeySequence::Delete, m_dosenView);
    connect(delDosenShortcut, &QShortcut::activated, this,
            &MainWindow::onDeleteDosen);

    // ---- Tab Ruangan ----
    auto* roomPage = new QWidget(this);
    auto* roomLayout = new QVBoxLayout(roomPage);
    auto* roomBtnRow = new QHBoxLayout;
    auto* addRoomBtn = new QPushButton("Tambah", roomPage);
    auto* editRoomBtn = new QPushButton("Ubah", roomPage);
    auto* delRoomBtn = new QPushButton("Hapus", roomPage);
    roomBtnRow->addWidget(addRoomBtn);
    roomBtnRow->addWidget(editRoomBtn);
    roomBtnRow->addWidget(delRoomBtn);
    roomBtnRow->addStretch();
    roomLayout->addLayout(roomBtnRow);
    m_roomModel = new QStandardItemModel(this);
    m_roomModel->setHorizontalHeaderLabels({"Nama Ruangan", "Kapasitas", "VCR"});
    m_roomModel->setSortRole(Qt::UserRole);
    m_roomView = new QTableView(this);
    m_roomView->setModel(m_roomModel);
    m_roomView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_roomView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_roomView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_roomView->setSortingEnabled(true);
    m_roomView->setAlternatingRowColors(true);
    m_roomView->verticalHeader()->setVisible(false);
    m_roomView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_roomView->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_roomView->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    roomLayout->addWidget(m_roomView);
    m_tabs->addTab(roomPage, "Ruangan");
    connect(addRoomBtn, &QPushButton::clicked, this, &MainWindow::onAddRoom);
    connect(editRoomBtn, &QPushButton::clicked, this, &MainWindow::onEditRoom);
    connect(delRoomBtn, &QPushButton::clicked, this, &MainWindow::onDeleteRoom);
    connect(m_roomView, &QTableView::doubleClicked, this,
            &MainWindow::onRoomDoubleClicked);
    auto* delRoomShortcut = new QShortcut(QKeySequence::Delete, m_roomView);
    connect(delRoomShortcut, &QShortcut::activated, this,
            &MainWindow::onDeleteRoom);

    // ---- Tab Hasil Jadwal ----
    auto* resultPage = new QWidget(this);
    auto* resultLayout = new QVBoxLayout(resultPage);
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel("Tampilkan:", resultPage));
    m_viewCombo = new QComboBox(resultPage);
    m_viewCombo->addItem("Jadwal Optimasi (Dinic)");
    m_viewCombo->addItem("Jadwal Awal (Welsh-Powell)");
    filterRow->addWidget(m_viewCombo);
    filterRow->addWidget(new QLabel("Program studi:", resultPage));
    m_prodiCombo = new QComboBox(resultPage);
    filterRow->addWidget(m_prodiCombo);
    m_searchEdit = new QLineEdit(resultPage);
    m_searchEdit->setPlaceholderText("Cari kode, mata kuliah, atau dosen...");
    m_searchEdit->setClearButtonEnabled(true);
    filterRow->addWidget(m_searchEdit, 1);
    m_conflictOnlyCheck = new QCheckBox("Hanya yang bermasalah", resultPage);
    m_conflictOnlyCheck->setToolTip(
        "Tampilkan hanya baris yang bentrok atau tanpa ruang yang muat.");
    filterRow->addWidget(m_conflictOnlyCheck);
    resultLayout->addLayout(filterRow);
    auto* legendLabel = new QLabel(resultPage);
    legendLabel->setTextFormat(Qt::RichText);
    legendLabel->setText(
        "Keterangan warna: "
        "<span style=\"background-color:#FECACA;\">&nbsp;&nbsp;&nbsp;</span> "
        "bentrok &nbsp;&nbsp; "
        "<span style=\"background-color:#FDE68A;\">&nbsp;&nbsp;&nbsp;</span> "
        "tidak ada ruang yang muat &nbsp;&nbsp; "
        "<span style=\"background-color:#FFFFFF; border:1px solid #999999;\">"
        "&nbsp;&nbsp;&nbsp;</span> aman");
    resultLayout->addWidget(legendLabel);
    m_summaryLabel = new QLabel(resultPage);
    m_summaryLabel->setWordWrap(true);
    resultLayout->addWidget(m_summaryLabel);
    m_resultModel = new QStandardItemModel(this);
    m_resultModel->setHorizontalHeaderLabels({"Waktu", "Kode", "Mata Kuliah",
                                              "Kelas", "Program Studi", "Dosen",
                                              "Ruang", "Status"});
    m_resultModel->setSortRole(Qt::UserRole);
    m_resultView = new QTableView(this);
    m_resultView->setModel(m_resultModel);
    m_resultView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultView->setSortingEnabled(true);
    m_resultView->setAlternatingRowColors(true);
    m_resultView->verticalHeader()->setVisible(false);
    m_resultView->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    m_resultView->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_resultView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_resultView->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    m_resultView->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::ResizeToContents);
    m_resultView->horizontalHeader()->setSectionResizeMode(
        5, QHeaderView::ResizeToContents);
    m_resultView->horizontalHeader()->setSectionResizeMode(
        6, QHeaderView::ResizeToContents);
    m_resultView->horizontalHeader()->setSectionResizeMode(
        7, QHeaderView::ResizeToContents);
    resultLayout->addWidget(m_resultView);
    auto* detailBox = new QGroupBox("Penjelasan baris terpilih", resultPage);
    m_detailLabel = new QLabel(detailBox);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto* detailLayout = new QVBoxLayout(detailBox);
    detailLayout->addWidget(m_detailLabel);
    detailBox->setMinimumHeight(84);
    resultLayout->addWidget(detailBox);
    m_tabs->addTab(resultPage, "Hasil Jadwal");
    connect(m_viewCombo, &QComboBox::currentIndexChanged, this,
            &MainWindow::onResultViewChanged);
    connect(m_prodiCombo, &QComboBox::currentIndexChanged, this,
            &MainWindow::onResultFilterChanged);
    connect(m_searchEdit, &QLineEdit::textChanged, this,
            &MainWindow::onResultFilterChanged);
    connect(m_conflictOnlyCheck, &QCheckBox::toggled, this,
            &MainWindow::onResultFilterChanged);
    connect(m_resultView->selectionModel(),
            &QItemSelectionModel::currentChanged, this,
            &MainWindow::onResultSelectionChanged);

    m_countLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_countLabel);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmDiscard("menutup aplikasi")) {
        event->accept();
    } else {
        event->ignore();
    }
}

std::vector<std::string> MainWindow::distinctProdi() const {
    std::vector<std::string> out;
    for (const auto& c : m_courses) {
        if (std::find(out.begin(), out.end(), c.programStudi) == out.end())
            out.push_back(c.programStudi);
    }
    std::sort(out.begin(), out.end());
    return out;
}

void MainWindow::refreshProdiFilter() {
    m_prodiCombo->blockSignals(true);
    m_prodiCombo->clear();
    m_prodiCombo->addItem("Semua program studi", QString());
    for (const auto& p : distinctProdi())
        m_prodiCombo->addItem(QString::fromStdString(p) + " - " +
                                  QString::fromStdString(prodiName(p)),
                              QString::fromStdString(p));
    m_prodiCombo->blockSignals(false);
}

void MainWindow::refreshCourseTable() {
    m_courseModel->setRowCount(0);
    for (size_t i = 0; i < m_courses.size(); i++) {
        const CourseData& c = m_courses[i];
        const int idx = static_cast<int>(i);
        QList<QStandardItem*> row;
        row << makeItem(QString::fromStdString(c.kode),
                        QString::fromStdString(c.kode), idx)
            << makeItem(QString::fromStdString(c.nama),
                        QString::fromStdString(c.nama), idx)
            << makeItem(QString::fromStdString(c.kelas),
                        QString::fromStdString(c.kelas), idx)
            << makeItem(QString::fromStdString(c.programStudi),
                        QString::fromStdString(c.programStudi), idx)
            << makeItem(QString::fromStdString(c.dosen),
                        QString::fromStdString(c.dosen), idx)
            << makeItem(QString::number(c.mhs), c.mhs, idx);
        m_courseModel->appendRow(row);
    }
    updateCounts();
}

void MainWindow::refreshDosenTable() {
    m_dosenModel->setRowCount(0);
    for (size_t i = 0; i < m_dosens.size(); i++) {
        const Lecturer& d = m_dosens[i];
        const int idx = static_cast<int>(i);
        QList<QStandardItem*> row;
        row << makeItem(QString::fromStdString(d.nama),
                        QString::fromStdString(d.nama), d.id)
            << makeItem(QString::number(lecturerUsageCount(d.id)),
                        lecturerUsageCount(d.id), idx);
        m_dosenModel->appendRow(row);
    }
    updateCounts();
}

void MainWindow::refreshRoomTable() {
    m_roomModel->setRowCount(0);
    for (size_t i = 0; i < m_rooms.size(); i++) {
        const RoomData& r = m_rooms[i];
        const int idx = static_cast<int>(i);
        QList<QStandardItem*> row;
        row << makeItem(QString::fromStdString(r.nama),
                        QString::fromStdString(r.nama), idx)
            << makeItem(QString::number(r.kapasitas), r.kapasitas, idx)
            << makeItem(r.vcr ? "Ya" : "-", r.vcr ? 1 : 0, idx);
        m_roomModel->appendRow(row);
    }
    updateCounts();
}

std::vector<const ScheduleRow*> MainWindow::filteredResultRows() const {
    std::vector<const ScheduleRow*> out;
    if (!m_hasResult) return out;
    const ScheduleResult& res =
        (m_viewCombo->currentIndex() == 1) ? m_wpResult : m_dinicResult;
    const QString prodiFilter = m_prodiCombo->currentData().toString();
    const QString query = m_searchEdit->text().trimmed().toLower();
    const bool onlyProblematic = m_conflictOnlyCheck->isChecked();
    for (const auto& row : res.rows) {
        if (onlyProblematic && !row.bentrokRuang && !row.bentrokKelas &&
            !row.bentrokDosen && !row.tanpaRuang && row.slot >= 0)
            continue;
        const CourseData& c = m_courses.at(static_cast<size_t>(row.courseIndex));
        if (!prodiFilter.isEmpty() &&
            QString::fromStdString(c.programStudi) != prodiFilter)
            continue;
        if (!query.isEmpty()) {
            const QString haystack =
                (QString::fromStdString(c.kode) + " " +
                 QString::fromStdString(c.nama) + " " +
                 QString::fromStdString(c.dosen))
                    .toLower();
            if (!haystack.contains(query)) continue;
        }
        out.push_back(&row);
    }
    return out;
}

void MainWindow::refreshResultTable() {
    m_resultModel->setRowCount(0);
    if (!m_hasResult) return;
    for (const ScheduleRow* rowPtr : filteredResultRows()) {
        const ScheduleRow& row = *rowPtr;
        const CourseData& c = m_courses.at(static_cast<size_t>(row.courseIndex));
        const QString waktu =
            (row.slot < 0) ? "Tidak terjadwal"
                           : QString::fromStdString(slotLabel(row.slot));
        QString ruangText = "Tidak ada ruang muat";
        if (row.room >= 0 && row.room < static_cast<int>(m_rooms.size()))
            ruangText = QString::fromStdString(m_rooms.at(static_cast<size_t>(row.room)).nama);
        const QString status = statusText(row, m_courses);
        const int idx = row.courseIndex;
        QList<QStandardItem*> items;
        items << makeItem(waktu, row.slot, idx)
              << makeItem(QString::fromStdString(c.kode),
                          QString::fromStdString(c.kode), idx)
              << makeItem(QString::fromStdString(c.nama),
                          QString::fromStdString(c.nama), idx)
              << makeItem(QString::fromStdString(c.kelas),
                          QString::fromStdString(c.kelas), idx)
              << makeItem(QString::fromStdString(c.programStudi),
                          QString::fromStdString(c.programStudi), idx)
              << makeItem(QString::fromStdString(c.dosen),
                          QString::fromStdString(c.dosen), idx)
              << makeItem(ruangText, ruangText, idx)
              << makeItem(status, status, idx);
        if (row.bentrokRuang || row.bentrokKelas || row.bentrokDosen) {
            for (auto* it : items) it->setBackground(QBrush(kConflictBg));
        } else if (row.tanpaRuang) {
            for (auto* it : items) it->setBackground(QBrush(kNoRoomBg));
        }
        m_resultModel->appendRow(items);
    }
    m_resultModel->sort(0, Qt::AscendingOrder);
    updateDetail();
}

void MainWindow::refreshSummary() {
    QString text;
    QString color;
    if (!m_hasResult) {
        if (m_resultsStale) {
            text =
                "Data berubah sejak penjadwalan terakhir. Klik Jadwalkan "
                "Otomatis untuk menghitung ulang.";
            color = "#92400E";
        } else {
            text =
                "Belum ada jadwal. Isi data mata kuliah dan ruangan, lalu klik "
                "Jadwalkan Otomatis.";
        }
    } else {
        const bool isWp = (m_viewCombo->currentIndex() == 1);
        const ScheduleResult& res = isWp ? m_wpResult : m_dinicResult;
        if (!res.feasible) {
            text =
                "Tidak dapat menyusun jadwal dalam 20 slot (Senin-Jumat, 4 jam "
                "per hari). Kurangi mata kuliah atau tambah ruangan, lalu "
                "jadwalkan ulang.";
            color = "#B91C1C";
        } else if (isWp) {
            text = QString(
                       "Jadwal awal (Welsh-Powell): %1 warna/slot, %2 mata "
                       "kuliah, %3 bentrok ruang, %4 bentrok kelas, %5 "
                       "bentrok dosen, %6 tanpa ruang. Status: %7.")
                       .arg(res.jumlahSlot)
                       .arg(m_courses.size())
                       .arg(res.bentrokRuang)
                       .arg(res.bentrokKelas)
                       .arg(res.bentrokDosen)
                       .arg(res.tanpaRuang)
                       .arg(res.valid ? "VALID" : "ADA BENTROK");
            color = res.valid ? "#166534" : "#B91C1C";
        } else {
            text = QString(
                       "Jadwal optimasi (Dinic): %1 slot dipakai (batas bawah "
                       "%2, grup terbesar %3), %4 mata kuliah, %5 bentrok "
                       "ruang, %6 bentrok kelas, %7 bentrok dosen, %8 tanpa "
                       "ruang yang muat. Status: %9.")
                       .arg(res.jumlahSlot)
                       .arg(res.batasBawah)
                       .arg(res.maksGrup)
                       .arg(m_courses.size())
                       .arg(res.bentrokRuang)
                       .arg(res.bentrokKelas)
                       .arg(res.bentrokDosen)
                       .arg(res.tanpaRuang)
                       .arg(res.valid ? "VALID, BEBAS BENTROK" : "BELUM VALID");
            color = res.valid ? "#166534"
                              : (res.tanpaRuang > 0 ? "#92400E" : "#B91C1C");
        }
    }
    if (m_hasResult) {
        const bool isWp = (m_viewCombo->currentIndex() == 1);
        const ScheduleResult& res = isWp ? m_wpResult : m_dinicResult;
        if (res.feasible && res.bentrokRuang > 0) {
            std::map<std::pair<int, int>, std::vector<int>> grup;
            for (const auto& row : res.rows) {
                if (row.slot >= 0 && row.room >= 0)
                    grup[{row.slot, row.room}].push_back(row.courseIndex);
            }
            for (const auto& kv : grup) {
                if (kv.second.size() <= 1) continue;
                QStringList codes;
                for (int ci : kv.second)
                    codes << QString::fromStdString(
                        m_courses.at(static_cast<size_t>(ci)).kode);
                const QString roomName = QString::fromStdString(
                    m_rooms.at(static_cast<size_t>(kv.first.second)).nama);
                text += QString(" Contoh: %1 di %2 diperebutkan %3 mata "
                                "kuliah (%4).")
                            .arg(QString::fromStdString(
                                     slotLabel(kv.first.first)),
                                 roomName)
                            .arg(kv.second.size())
                            .arg(codes.join(", "));
                break;
            }
        }
        if (isWp && res.feasible && res.jumlahSlot > 20) {
            text += QString(" Jadwal awal butuh %1 slot, melebihi 20 slot "
                            "yang tersedia dalam seminggu (Senin-Jumat).")
                        .arg(res.jumlahSlot);
        }
        if (res.feasible && res.bentrokDosen > 0) {
            for (const auto& row : res.rows) {
                if (row.konflikDosenDengan.empty()) continue;
                const CourseData& c0 =
                    m_courses.at(static_cast<size_t>(row.courseIndex));
                const CourseData& c1 = m_courses.at(
                    static_cast<size_t>(row.konflikDosenDengan.front()));
                text += QString(" Contoh bentrok dosen: %1 mengajar %2 dan "
                                "%3 pada %4.")
                            .arg(QString::fromStdString(c0.dosen))
                            .arg(QString::fromStdString(c0.kode))
                            .arg(QString::fromStdString(c1.kode))
                            .arg(QString::fromStdString(slotLabel(row.slot)));
                break;
            }
            // Satu dosen tidak bisa mengajar dua mata kuliah pada satu slot,
            // jadi dosen dengan beban di atas 20 mata kuliah membuat jadwal
            // bebas bentrok dosen mustahil dalam 20 slot seminggu.
            std::map<int, int> beban;
            for (const auto& c : m_courses)
                if (c.dosenId >= 0) beban[c.dosenId]++;
            int bebanTerbesar = 0;
            int idTerbesar = -1;
            for (const auto& kv : beban) {
                if (kv.second > bebanTerbesar) {
                    bebanTerbesar = kv.second;
                    idTerbesar = kv.first;
                }
            }
            if (bebanTerbesar > 20 && idTerbesar >= 0) {
                text += QString(" %1 mengajar %2 mata kuliah, sehingga "
                                "jadwal yang sepenuhnya bebas bentrok dosen "
                                "butuh lebih dari 20 slot.")
                            .arg(QString::fromStdString(dosenName(idTerbesar)))
                            .arg(bebanTerbesar);
            }
        }
    }
    m_summaryLabel->setText(text);
    m_summaryLabel->setStyleSheet(color.isEmpty()
                                      ? QString()
                                      : "color: " + color + "; font-weight: bold;");
}

void MainWindow::onResultSelectionChanged() { updateDetail(); }

void MainWindow::updateDetail() {
    if (!m_hasResult) {
        m_detailLabel->setText(
            "Jalankan penjadwalan otomatis dulu, lalu pilih salah satu baris "
            "di tabel untuk melihat penjelasannya.");
        return;
    }
    const QModelIndex idx = m_resultView->currentIndex();
    const QStandardItem* item =
        idx.isValid() ? m_resultModel->item(idx.row(), 0) : nullptr;
    if (!item) {
        m_detailLabel->setText(
            "Pilih salah satu baris di tabel untuk melihat penjelasan kenapa "
            "baris itu aman atau bentrok, dan bentroknya dengan siapa.");
        return;
    }
    const int courseIndex = item->data(kIndexRole).toInt();
    const ScheduleResult& res =
        (m_viewCombo->currentIndex() == 1) ? m_wpResult : m_dinicResult;
    const ScheduleRow* target = nullptr;
    for (const auto& row : res.rows) {
        if (row.courseIndex == courseIndex) {
            target = &row;
            break;
        }
    }
    if (!target) {
        m_detailLabel->setText("Baris tidak ditemukan.");
        return;
    }
    m_detailLabel->setText(detailText(*target));
}

QString MainWindow::detailText(const ScheduleRow& row) const {
    const CourseData& c = m_courses.at(static_cast<size_t>(row.courseIndex));
    const QString kode = QString::fromStdString(c.kode);
    if (row.slot < 0) {
        return QString(
                   "%1 tidak terjadwal. Tidak ada slot kosong yang tersisa "
                   "dalam 20 slot (Senin sampai Jumat, 4 jam per hari). "
                   "Kurangi mata kuliah atau tambah ruangan, lalu jadwalkan "
                   "ulang.")
            .arg(kode);
    }
    const QString waktu = QString::fromStdString(slotLabel(row.slot));
    auto describeWho = [this](const std::vector<int>& indices) {
        QStringList daftar;
        for (int ci : indices) {
            const CourseData& o = m_courses.at(static_cast<size_t>(ci));
            daftar << QString("%1 (%2 kelas %3)")
                          .arg(QString::fromStdString(o.kode),
                               QString::fromStdString(o.programStudi),
                               QString::fromStdString(o.kelas));
        }
        return daftar;
    };
    QStringList sentences;
    if (row.bentrokRuang) {
        QStringList daftar;
        daftar << QString("%1 (%2 kelas %3)")
                      .arg(kode, QString::fromStdString(c.programStudi),
                           QString::fromStdString(c.kelas));
        daftar += describeWho(row.konflikRuangDengan);
        const QString ruang = QString::fromStdString(
            m_rooms.at(static_cast<size_t>(row.room)).nama);
        sentences << QString(
                         "Bentrok ruang: ruangan %1 pada %2 dipakai oleh %3 "
                         "mata kuliah: %4. Satu ruangan hanya bisa dipakai "
                         "oleh satu mata kuliah dalam satu waktu.")
                         .arg(ruang, waktu)
                         .arg(daftar.size())
                         .arg(daftar.join(", "));
    }
    if (row.bentrokKelas) {
        QStringList daftar;
        daftar << QString("%1 (%2 kelas %3)")
                      .arg(kode, QString::fromStdString(c.programStudi),
                           QString::fromStdString(c.kelas));
        daftar += describeWho(row.konflikKelasDengan);
        sentences << QString(
                         "Bentrok kelas: %1 mata kuliah untuk %2 kelas %3 "
                         "dijadwalkan pada %4: %5. Mahasiswa kelas itu tidak "
                         "bisa hadir di dua tempat sekaligus.")
                         .arg(daftar.size())
                         .arg(QString::fromStdString(c.programStudi),
                              QString::fromStdString(c.kelas), waktu)
                         .arg(daftar.join(", "));
    }
    if (row.bentrokDosen) {
        QStringList daftar;
        daftar << QString("%1 (%2 kelas %3)")
                      .arg(kode, QString::fromStdString(c.programStudi),
                           QString::fromStdString(c.kelas));
        daftar += describeWho(row.konflikDosenDengan);
        sentences << QString(
                         "Bentrok dosen: %1 mengajar %2 mata kuliah pada %3: "
                         "%4. Satu dosen tidak bisa mengajar dua kelas pada "
                         "waktu yang sama.")
                         .arg(QString::fromStdString(c.dosen))
                         .arg(daftar.size())
                         .arg(waktu)
                         .arg(daftar.join(", "));
    }
    if (row.tanpaRuang) {
        const ScheduleResult& res =
            (m_viewCombo->currentIndex() == 1) ? m_wpResult : m_dinicResult;
        if (c.mhs >= 40) {
            // Mata kuliah besar wajib menempati ruangan VCR.
            bool adaVcr = false;
            int kapasitasVcrTerbesar = 0;
            for (const auto& r : m_rooms) {
                if (!r.vcr) continue;
                adaVcr = true;
                kapasitasVcrTerbesar =
                    std::max(kapasitasVcrTerbesar, r.kapasitas);
            }
            if (!adaVcr) {
                sentences << QString(
                                 "Tidak ada ruang yang muat: mata kuliah ini "
                                 "diikuti %1 mahasiswa (40 atau lebih), "
                                 "sehingga harus di ruangan VCR, tetapi "
                                 "tidak ada ruangan VCR yang terdaftar. "
                                 "Tandai satu ruangan sebagai VCR di tab "
                                 "Ruangan.")
                                 .arg(c.mhs);
            } else if (c.mhs > kapasitasVcrTerbesar) {
                sentences << QString(
                                 "Tidak ada ruang yang muat: mata kuliah ini "
                                 "diikuti %1 mahasiswa, sedangkan ruangan VCR "
                                 "terbesar hanya berkapasitas %2.")
                                 .arg(c.mhs)
                                 .arg(kapasitasVcrTerbesar);
            } else {
                QStringList pemakai;
                for (const auto& other : res.rows) {
                    if (other.courseIndex == row.courseIndex ||
                        other.slot != row.slot || other.room < 0)
                        continue;
                    if (m_rooms.at(static_cast<size_t>(other.room)).vcr)
                        pemakai << QString::fromStdString(
                            m_courses.at(static_cast<size_t>(other.courseIndex))
                                .kode);
                }
                if (!pemakai.isEmpty()) {
                    sentences << QString(
                                     "Tidak ada ruang yang muat: mata kuliah "
                                     "ini diikuti %1 mahasiswa sehingga harus "
                                     "di ruangan VCR, tetapi ruangan VCR "
                                     "sudah dipakai %2 pada %3.")
                                     .arg(c.mhs)
                                     .arg(pemakai.join(", "))
                                     .arg(waktu);
                } else {
                    sentences << QString(
                                     "Tidak ada ruang VCR yang tersedia pada "
                                     "%1 untuk mata kuliah ini.")
                                     .arg(waktu);
                }
            }
        } else {
            bool adaBiasa = false;
            bool adaBiasaMuat = false;
            int kapasitasBiasaTerbesar = 0;
            for (const auto& r : m_rooms) {
                if (r.vcr) continue;
                adaBiasa = true;
                kapasitasBiasaTerbesar =
                    std::max(kapasitasBiasaTerbesar, r.kapasitas);
                if (r.kapasitas >= c.mhs) adaBiasaMuat = true;
            }
            if (!adaBiasa) {
                sentences << "Tidak ada ruangan biasa yang terdaftar. "
                             "Tambahkan ruangan di tab Ruangan.";
            } else if (!adaBiasaMuat) {
                sentences << QString(
                                 "Tidak ada ruang yang muat: mata kuliah ini "
                                 "diikuti %1 mahasiswa, sedangkan ruangan "
                                 "biasa terbesar hanya berkapasitas %2. "
                                 "Tambahkan ruangan yang lebih besar atau "
                                 "kurangi jumlah mahasiswa.")
                                 .arg(c.mhs)
                                 .arg(kapasitasBiasaTerbesar);
            } else {
                sentences << QString(
                                 "Semua ruangan biasa yang muat untuk %1 "
                                 "mahasiswa sudah dipakai mata kuliah lain "
                                 "pada %2.")
                                 .arg(c.mhs)
                                 .arg(waktu);
            }
        }
    }
    if (sentences.isEmpty()) {
        const QString ruang =
            (row.room >= 0)
                ? QString::fromStdString(
                      m_rooms.at(static_cast<size_t>(row.room)).nama)
                : "tanpa ruangan";
        sentences << QString(
                         "Aman, tidak ada bentrok. %1 dijadwalkan %2 di %3. "
                         "Tidak ada mata kuliah lain di ruangan dan waktu "
                         "yang sama, dan tidak ada mata kuliah sekelas maupun "
                         "sedosen yang bentrok.")
                         .arg(kode, waktu, ruang);
    } else if (m_viewCombo->currentIndex() == 1) {
        sentences << "Lihat Jadwal Optimasi (Dinic) untuk jadwal yang sudah "
                     "dibagi agar bentroknya hilang.";
    }
    return sentences.join(" ");
}

void MainWindow::updateCounts() {
    m_countLabel->setText(QString("%1 mata kuliah | %2 dosen | %3 ruangan")
                              .arg(m_courses.size())
                              .arg(m_dosens.size())
                              .arg(m_rooms.size()));
}

void MainWindow::setDirty(bool dirty) {
    m_dirty = dirty;
    setWindowTitle(QString("Penjadwalan Mata Kuliah") + (dirty ? " *" : ""));
}

void MainWindow::invalidateResults() {
    if (m_hasResult) {
        m_hasResult = false;
        m_resultsStale = true;
    }
    refreshResultTable();
    refreshSummary();
}

bool MainWindow::confirmDiscard(const QString& aksi) {
    if (!m_dirty) return true;
    const auto answer =
        QMessageBox::question(this, "Perubahan belum disimpan",
                              "Data berubah sejak terakhir disimpan. Simpan dulu "
                              "sebelum " +
                                  aksi + "?",
                              QMessageBox::Save | QMessageBox::Discard |
                                  QMessageBox::Cancel,
                              QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Discard) return true;
    // Save
    if (m_currentFile.isEmpty()) {
        onSaveFileAs();
        return !m_dirty;
    }
    onSaveFile();
    return !m_dirty;
}

bool MainWindow::saveToFile(const QString& path) {
    QJsonObject root;
    root["version"] = 2;
    QJsonArray courses;
    for (const auto& c : m_courses) {
        QJsonObject o;
        o["kode"] = QString::fromStdString(c.kode);
        o["nama"] = QString::fromStdString(c.nama);
        o["kelas"] = QString::fromStdString(c.kelas);
        o["programStudi"] = QString::fromStdString(c.programStudi);
        o["dosen"] = QString::fromStdString(c.dosen);
        o["dosenId"] = c.dosenId;
        o["mhs"] = c.mhs;
        courses.append(o);
    }
    QJsonArray dosens;
    for (const auto& d : m_dosens) {
        QJsonObject o;
        o["id"] = d.id;
        o["nama"] = QString::fromStdString(d.nama);
        dosens.append(o);
    }
    QJsonArray rooms;
    for (const auto& r : m_rooms) {
        QJsonObject o;
        o["nama"] = QString::fromStdString(r.nama);
        o["kapasitas"] = r.kapasitas;
        o["vcr"] = r.vcr;
        rooms.append(o);
    }
    root["mataKuliah"] = courses;
    root["dosen"] = dosens;
    root["ruang"] = rooms;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    return true;
}

bool MainWindow::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject root = doc.object();
    if (!root["mataKuliah"].isArray() || !root["ruang"].isArray()) return false;

    auto readString = [](const QJsonObject& o, const char* key, std::string& out) {
        if (!o[key].isString()) return false;
        out = o[key].toString().toStdString();
        return !out.empty();
    };
    auto readInt = [](const QJsonObject& o, const char* key, int& out) {
        if (!o[key].isDouble()) return false;
        out = o[key].toInt();
        return out > 0;
    };

    std::vector<Lecturer> dosens;
    std::map<int, std::string> namaById;
    if (root["dosen"].isArray()) {
        for (const QJsonValue& v : root["dosen"].toArray()) {
            if (!v.isObject()) return false;
            const QJsonObject o = v.toObject();
            Lecturer d;
            std::string nama;
            if (!o["id"].isDouble() || !readString(o, "nama", nama))
                return false;
            d.id = o["id"].toInt();
            d.nama = nama;
            if (d.id <= 0 || namaById.count(d.id) != 0) return false;
            namaById[d.id] = d.nama;
            dosens.push_back(d);
        }
    }
    int nextId = 1;
    for (const auto& d : dosens) nextId = std::max(nextId, d.id + 1);
    auto idForName = [&](const std::string& nama) {
        for (const auto& d : dosens) {
            if (QString::compare(QString::fromStdString(d.nama),
                                 QString::fromStdString(nama),
                                 Qt::CaseInsensitive) == 0)
                return d.id;
        }
        Lecturer baru{nextId, nama};
        nextId++;
        dosens.push_back(baru);
        namaById[baru.id] = baru.nama;
        return baru.id;
    };

    std::vector<CourseData> courses;
    for (const QJsonValue& v : root["mataKuliah"].toArray()) {
        if (!v.isObject()) return false;
        const QJsonObject o = v.toObject();
        CourseData c;
        if (!readString(o, "kode", c.kode) || !readString(o, "nama", c.nama) ||
            !readString(o, "kelas", c.kelas) ||
            !readString(o, "programStudi", c.programStudi) ||
            !readString(o, "dosen", c.dosen) || !readInt(o, "mhs", c.mhs))
            return false;
        if (o["dosenId"].isDouble() &&
            namaById.count(o["dosenId"].toInt()) != 0) {
            c.dosenId = o["dosenId"].toInt();
            c.dosen = namaById[c.dosenId];
        } else {
            c.dosenId = idForName(c.dosen);
            c.dosen = namaById[c.dosenId];
        }
        courses.push_back(c);
    }
    std::vector<RoomData> rooms;
    for (const QJsonValue& v : root["ruang"].toArray()) {
        if (!v.isObject()) return false;
        const QJsonObject o = v.toObject();
        RoomData r;
        if (!readString(o, "nama", r.nama) || !readInt(o, "kapasitas", r.kapasitas))
            return false;
        if (o["vcr"].isBool()) {
            r.vcr = o["vcr"].toBool();
        } else {
            // File lama tanpa flag: kenali VCR dari namanya.
            r.vcr = QString::fromStdString(r.nama).contains("VCR",
                                                           Qt::CaseInsensitive);
        }
        rooms.push_back(r);
    }
    m_courses = courses;
    m_dosens = dosens;
    m_nextDosenId = nextId;
    m_rooms = rooms;
    return true;
}

int MainWindow::selectedCourseIndex() const {
    const QModelIndex idx = m_courseView->currentIndex();
    if (!idx.isValid()) return -1;
    const QStandardItem* item = m_courseModel->item(idx.row(), 0);
    if (!item) return -1;
    const int courseIndex = item->data(kIndexRole).toInt();
    if (courseIndex < 0 || courseIndex >= static_cast<int>(m_courses.size()))
        return -1;
    return courseIndex;
}

int MainWindow::selectedRoomIndex() const {
    const QModelIndex idx = m_roomView->currentIndex();
    if (!idx.isValid()) return -1;
    const QStandardItem* item = m_roomModel->item(idx.row(), 0);
    if (!item) return -1;
    const int roomIndex = item->data(kIndexRole).toInt();
    if (roomIndex < 0 || roomIndex >= static_cast<int>(m_rooms.size())) return -1;
    return roomIndex;
}

std::string MainWindow::dosenName(int dosenId) const {
    for (const auto& d : m_dosens)
        if (d.id == dosenId) return d.nama;
    return std::string();
}

void MainWindow::syncDosenNames() {
    for (auto& c : m_courses) {
        const std::string nama = dosenName(c.dosenId);
        if (!nama.empty()) c.dosen = nama;
    }
}

int MainWindow::lecturerUsageCount(int dosenId) const {
    int n = 0;
    for (const auto& c : m_courses)
        if (c.dosenId == dosenId) n++;
    return n;
}

int MainWindow::addLecturerByName(const std::string& nama) {
    const QString clean = QString::fromStdString(nama).trimmed();
    if (clean.isEmpty()) return -1;
    for (const auto& d : m_dosens) {
        if (QString::compare(QString::fromStdString(d.nama), clean,
                             Qt::CaseInsensitive) == 0)
            return d.id;
    }
    const int id = m_nextDosenId++;
    m_dosens.push_back({id, clean.toStdString()});
    setDirty(true);
    invalidateResults();
    refreshDosenTable();
    return id;
}

int MainWindow::selectedDosenPosition() const {
    const QModelIndex idx = m_dosenView->currentIndex();
    if (!idx.isValid()) return -1;
    const QStandardItem* item = m_dosenModel->item(idx.row(), 0);
    if (!item) return -1;
    const int dosenId = item->data(kIndexRole).toInt();
    for (size_t i = 0; i < m_dosens.size(); i++)
        if (m_dosens[i].id == dosenId) return static_cast<int>(i);
    return -1;
}

bool MainWindow::kodeExists(const std::string& kode, int exceptIndex) const {
    for (size_t i = 0; i < m_courses.size(); i++) {
        if (static_cast<int>(i) == exceptIndex) continue;
        if (m_courses[i].kode == kode) return true;
    }
    return false;
}

void MainWindow::onNewFile() {
    if (!confirmDiscard("membuat data baru")) return;
    m_courses.clear();
    m_dosens.clear();
    m_nextDosenId = 1;
    m_rooms.clear();
    m_currentFile.clear();
    setDirty(false);
    invalidateResults();
    refreshCourseTable();
    refreshDosenTable();
    refreshRoomTable();
    refreshProdiFilter();
    statusBar()->showMessage("Data baru yang kosong.", 5000);
}

void MainWindow::onOpenFile() {
    if (!confirmDiscard("membuka file lain")) return;
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this, "Buka Data Jadwal", dir, "Data Jadwal JSON (*.json)");
    if (path.isEmpty()) return;
    if (!loadFromFile(path)) {
        QMessageBox::warning(this, "File tidak dapat dibaca",
                             "File tersebut bukan data jadwal yang valid.");
        return;
    }
    m_currentFile = path;
    setDirty(false);
    invalidateResults();
    refreshCourseTable();
    refreshDosenTable();
    refreshRoomTable();
    refreshProdiFilter();
    statusBar()->showMessage("Data dimuat dari " + path, 5000);
}

void MainWindow::onSaveFile() {
    if (m_currentFile.isEmpty()) {
        onSaveFileAs();
        return;
    }
    if (!saveToFile(m_currentFile)) {
        QMessageBox::warning(this, "Gagal menyimpan",
                             "Data tidak dapat disimpan ke " + m_currentFile);
        return;
    }
    setDirty(false);
    statusBar()->showMessage("Data disimpan.", 5000);
}

void MainWindow::onSaveFileAs() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(
        this, "Simpan Data Jadwal", dir, "Data Jadwal JSON (*.json)");
    if (path.isEmpty()) return;
    if (!saveToFile(path)) {
        QMessageBox::warning(this, "Gagal menyimpan",
                             "Data tidak dapat disimpan ke " + path);
        return;
    }
    m_currentFile = path;
    setDirty(false);
    statusBar()->showMessage("Data disimpan ke " + path, 5000);
}

void MainWindow::onExportCsv() {
    if (!m_hasResult) {
        QMessageBox::warning(this, "Belum ada hasil",
                             "Jalankan penjadwalan otomatis dulu sebelum "
                             "mengekspor hasil.");
        return;
    }
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(
        this, "Ekspor Hasil ke CSV", dir, "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Gagal mengekspor",
                             "Hasil tidak dapat ditulis ke " + path);
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QChar(0xFEFF);
    out << "Waktu,Kode,Mata Kuliah,Kelas,Program Studi,Dosen,Ruang,Status\n";
    for (const ScheduleRow* rowPtr : filteredResultRows()) {
        const ScheduleRow& row = *rowPtr;
        const CourseData& c = m_courses.at(static_cast<size_t>(row.courseIndex));
        QString ruangText = "Tidak ada ruang muat";
        if (row.room >= 0 && row.room < static_cast<int>(m_rooms.size()))
            ruangText =
                QString::fromStdString(m_rooms.at(static_cast<size_t>(row.room)).nama);
        const QString waktu =
            (row.slot < 0) ? "Tidak terjadwal"
                           : QString::fromStdString(slotLabel(row.slot));
        out << csvField(waktu) << "," << csvField(QString::fromStdString(c.kode))
            << "," << csvField(QString::fromStdString(c.nama)) << ","
            << csvField(QString::fromStdString(c.kelas)) << ","
            << csvField(QString::fromStdString(c.programStudi)) << ","
            << csvField(QString::fromStdString(c.dosen)) << ","
            << csvField(ruangText) << "," << csvField(statusText(row, m_courses)) << "\n";
    }
    statusBar()->showMessage("Hasil diekspor ke " + path, 5000);
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this, "Tentang Penjadwalan Mata Kuliah",
        "Aplikasi penjadwalan mata kuliah.\n\n"
        "Cara pakai: isi daftar mata kuliah, dosen, dan ruangan, lalu klik "
        "Jadwalkan Otomatis. Aplikasi menghitung jadwal awal dengan "
        "Welsh-Powell, kemudian mengoptimasinya dengan aliran maksimum "
        "Dinic.\n\n"
        "Aturan: mata kuliah sekelas dan sedosen tidak boleh satu slot; "
        "mata kuliah 40 mahasiswa atau lebih hanya menempati ruangan VCR.\n\n"
        "Algoritma yang dipakai sama dengan file asli efisiensibentrok.cpp "
        "(gabungan_jadwal_final.cpp), ditambah dua aturan di atas. Slot "
        "waktu: Senin-Jumat, 4 jam kuliah per hari.");
}

void MainWindow::onLoadSample() {
    if (!confirmDiscard("memuat data contoh")) return;
    m_courses = sampleCourses();
    m_dosens = sampleLecturers();
    m_nextDosenId = 1;
    for (const auto& d : m_dosens)
        m_nextDosenId = std::max(m_nextDosenId, d.id + 1);
    m_rooms = sampleRooms();
    m_currentFile.clear();
    setDirty(true);
    invalidateResults();
    refreshCourseTable();
    refreshDosenTable();
    refreshRoomTable();
    refreshProdiFilter();
    statusBar()->showMessage("Data contoh dimuat.", 5000);
}

void MainWindow::onClearAll() {
    if (!confirmDiscard("menghapus semua data")) return;
    m_courses.clear();
    m_dosens.clear();
    m_nextDosenId = 1;
    m_rooms.clear();
    setDirty(true);
    invalidateResults();
    refreshCourseTable();
    refreshDosenTable();
    refreshRoomTable();
    refreshProdiFilter();
    statusBar()->showMessage("Semua data dihapus.", 5000);
}

void MainWindow::onAddCourse() {
    CourseData data;
    data.mhs = 35;
    auto addLecturer = [this](const std::string& nama) {
        return addLecturerByName(nama);
    };
    if (!CourseDialog::editCourse(this, data, true, distinctProdi(), m_dosens,
                                  addLecturer))
        return;
    if (kodeExists(data.kode, -1)) {
        QMessageBox::warning(this, "Kode sudah dipakai",
                             "Kode " + QString::fromStdString(data.kode) +
                                 " sudah ada. Gunakan kode lain.");
        return;
    }
    m_courses.push_back(data);
    setDirty(true);
    invalidateResults();
    refreshCourseTable();
    refreshProdiFilter();
}

void MainWindow::onEditCourse() {
    const int idx = selectedCourseIndex();
    if (idx < 0) {
        QMessageBox::information(this, "Pilih dulu",
                                 "Pilih satu mata kuliah yang mau diubah.");
        return;
    }
    CourseData data = m_courses.at(static_cast<size_t>(idx));
    auto addLecturer = [this](const std::string& nama) {
        return addLecturerByName(nama);
    };
    if (!CourseDialog::editCourse(this, data, false, distinctProdi(), m_dosens,
                                  addLecturer))
        return;
    if (kodeExists(data.kode, idx)) {
        QMessageBox::warning(this, "Kode sudah dipakai",
                             "Kode " + QString::fromStdString(data.kode) +
                                 " sudah dipakai mata kuliah lain.");
        return;
    }
    m_courses[static_cast<size_t>(idx)] = data;
    setDirty(true);
    invalidateResults();
    refreshCourseTable();
    refreshProdiFilter();
}

void MainWindow::onDeleteCourse() {
    const int idx = selectedCourseIndex();
    if (idx < 0) return;
    const QString kode =
        QString::fromStdString(m_courses.at(static_cast<size_t>(idx)).kode);
    m_courses.erase(m_courses.begin() + idx);
    setDirty(true);
    invalidateResults();
    refreshCourseTable();
    refreshProdiFilter();
    statusBar()->showMessage("Mata kuliah " + kode + " dihapus.", 5000);
}

void MainWindow::onAddDosen() {
    Lecturer data;
    if (!LecturerDialog::editLecturer(this, data, true)) return;
    const QString nama = QString::fromStdString(data.nama).trimmed();
    for (const auto& d : m_dosens) {
        if (QString::compare(QString::fromStdString(d.nama), nama,
                             Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, "Dosen sudah ada",
                                 "Dosen " + nama + " sudah ada di daftar.");
            return;
        }
    }
    if (addLecturerByName(data.nama) < 0) return;
    statusBar()->showMessage("Dosen " + nama + " ditambahkan.", 5000);
}

void MainWindow::onEditDosen() {
    const int pos = selectedDosenPosition();
    if (pos < 0) {
        QMessageBox::information(this, "Pilih dulu",
                                 "Pilih satu dosen yang mau diubah.");
        return;
    }
    Lecturer data = m_dosens.at(static_cast<size_t>(pos));
    const int idLama = data.id;
    if (!LecturerDialog::editLecturer(this, data, false)) return;
    const QString namaBaru = QString::fromStdString(data.nama).trimmed();
    // Nama baru sudah dipakai dosen lain: tawarkan gabung, supaya tidak ada
    // dua identitas untuk orang yang sama (mis. "Sanggi Bayu A." dan
    // "Sanggi Bayu Ardika").
    for (const auto& d : m_dosens) {
        if (d.id == idLama) continue;
        if (QString::compare(QString::fromStdString(d.nama), namaBaru,
                             Qt::CaseInsensitive) != 0)
            continue;
        const int pakai = lecturerUsageCount(idLama);
        const auto answer = QMessageBox::question(
            this, "Gabungkan dosen?",
            "Dosen " + QString::fromStdString(d.nama) + " sudah ada. "
            "Gabungkan " + QString::fromStdString(m_dosens.at(static_cast<size_t>(pos)).nama) +
                " ke " + QString::fromStdString(d.nama) + "? " +
                QString::number(pakai) +
                " mata kuliah akan dipindahkan ke dosen tersebut.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        for (auto& c : m_courses) {
            if (c.dosenId == idLama) {
                c.dosenId = d.id;
                c.dosen = d.nama;
            }
        }
        m_dosens.erase(m_dosens.begin() + pos);
        setDirty(true);
        invalidateResults();
        refreshDosenTable();
        refreshCourseTable();
        statusBar()->showMessage(
            "Dosen digabungkan ke " + QString::fromStdString(d.nama) + ".", 5000);
        return;
    }
    m_dosens[static_cast<size_t>(pos)].nama = namaBaru.toStdString();
    syncDosenNames();
    setDirty(true);
    invalidateResults();
    refreshDosenTable();
    refreshCourseTable();
}

void MainWindow::onDeleteDosen() {
    const int pos = selectedDosenPosition();
    if (pos < 0) return;
    const Lecturer d = m_dosens.at(static_cast<size_t>(pos));
    const int pakai = lecturerUsageCount(d.id);
    if (pakai > 0) {
        QMessageBox::warning(
            this, "Dosen masih dipakai",
            "Dosen " + QString::fromStdString(d.nama) + " masih mengajar " +
                QString::number(pakai) +
                " mata kuliah. Ubah dulu mata kuliahnya ke dosen lain "
                "lewat tab Mata Kuliah, atau gabungkan lewat tombol Ubah.");
        return;
    }
    m_dosens.erase(m_dosens.begin() + pos);
    setDirty(true);
    invalidateResults();
    refreshDosenTable();
    statusBar()->showMessage(
        "Dosen " + QString::fromStdString(d.nama) + " dihapus.", 5000);
}

void MainWindow::onDosenDoubleClicked(const QModelIndex& index) {
    if (index.isValid()) onEditDosen();
}

void MainWindow::onAddRoom() {
    RoomData data;
    if (!RoomDialog::editRoom(this, data, true)) return;
    m_rooms.push_back(data);
    setDirty(true);
    invalidateResults();
    refreshRoomTable();
}

void MainWindow::onEditRoom() {
    const int idx = selectedRoomIndex();
    if (idx < 0) {
        QMessageBox::information(this, "Pilih dulu",
                                 "Pilih satu ruangan yang mau diubah.");
        return;
    }
    RoomData data = m_rooms.at(static_cast<size_t>(idx));
    if (!RoomDialog::editRoom(this, data, false)) return;
    m_rooms[static_cast<size_t>(idx)] = data;
    setDirty(true);
    invalidateResults();
    refreshRoomTable();
}

void MainWindow::onDeleteRoom() {
    const int idx = selectedRoomIndex();
    if (idx < 0) return;
    const QString nama =
        QString::fromStdString(m_rooms.at(static_cast<size_t>(idx)).nama);
    m_rooms.erase(m_rooms.begin() + idx);
    setDirty(true);
    invalidateResults();
    refreshRoomTable();
    statusBar()->showMessage("Ruangan " + nama + " dihapus.", 5000);
}

void MainWindow::onCourseDoubleClicked(const QModelIndex& index) {
    if (index.isValid()) onEditCourse();
}

void MainWindow::onRoomDoubleClicked(const QModelIndex& index) {
    if (index.isValid()) onEditRoom();
}

void MainWindow::onSchedule() {
    if (m_courses.empty()) {
        QMessageBox::warning(this, "Belum ada data",
                             "Tambahkan mata kuliah dulu sebelum menjadwalkan.");
        return;
    }
    if (m_rooms.empty()) {
        QMessageBox::warning(this, "Belum ada ruangan",
                             "Tambahkan minimal satu ruangan dulu sebelum "
                             "menjadwalkan.");
        return;
    }
    m_dinicResult = computeDinic(m_courses, m_rooms);
    m_wpResult = computeWelshPowell(m_courses, m_rooms);
    m_hasResult = true;
    m_resultsStale = false;
    if (qEnvironmentVariableIsSet("PENJADWALAN_QT_AUTOTEST")) {
        qDebug("AUTOTEST dinic: slots=%d batasBawah=%d bentrokRuang=%d "
               "bentrokKelas=%d bentrokDosen=%d tanpaRuang=%d feasible=%d "
               "valid=%d",
               m_dinicResult.jumlahSlot, m_dinicResult.batasBawah,
               m_dinicResult.bentrokRuang, m_dinicResult.bentrokKelas,
               m_dinicResult.bentrokDosen, m_dinicResult.tanpaRuang,
               static_cast<int>(m_dinicResult.feasible),
               static_cast<int>(m_dinicResult.valid));
        qDebug("AUTOTEST wp: slots=%d bentrokRuang=%d bentrokKelas=%d "
               "bentrokDosen=%d tanpaRuang=%d feasible=%d valid=%d",
               m_wpResult.jumlahSlot, m_wpResult.bentrokRuang,
               m_wpResult.bentrokKelas, m_wpResult.bentrokDosen,
               m_wpResult.tanpaRuang, static_cast<int>(m_wpResult.feasible),
               static_cast<int>(m_wpResult.valid));
    }
    m_viewCombo->setCurrentIndex(0);
    refreshResultTable();
    refreshSummary();
    m_tabs->setCurrentIndex(3);
    // Pastikan tabel hasil terlihat dari baris paling atas: layout tab baru
    // selesai setelah event loop berjalan, jadi scroll-nya ditunda.
    QTimer::singleShot(0, this, [this] { m_resultView->scrollToTop(); });
    if (m_dinicResult.feasible && m_dinicResult.valid) {
        statusBar()->showMessage(
            QString("Jadwal optimasi selesai: %1 slot, %2 mata kuliah, bebas "
                    "bentrok.")
                .arg(m_dinicResult.jumlahSlot)
                .arg(m_courses.size()),
            8000);
    } else if (m_dinicResult.feasible) {
        statusBar()->showMessage(
            "Jadwal dihitung, tetapi belum valid. Lihat ringkasan di tab Hasil "
            "Jadwal.",
            8000);
    } else {
        statusBar()->showMessage(
            "Jadwal tidak dapat disusun dalam 20 slot. Kurangi mata kuliah "
            "atau tambah ruangan.",
            8000);
    }
}

void MainWindow::onResultViewChanged() {
    refreshResultTable();
    refreshSummary();
}

void MainWindow::onResultFilterChanged() {
    refreshResultTable();
}
