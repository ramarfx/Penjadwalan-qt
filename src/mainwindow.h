#pragma once

#include "scheduler_core.h"

#include <QMainWindow>
#include <string>
#include <vector>

class QCloseEvent;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QStandardItemModel;
class QTabWidget;
class QTableView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void onSaveFileAs();
    void onExportCsv();
    void onAbout();
    void onLoadSample();
    void onClearAll();
    void onAddCourse();
    void onEditCourse();
    void onDeleteCourse();
    void onAddDosen();
    void onEditDosen();
    void onDeleteDosen();
    void onAddRoom();
    void onEditRoom();
    void onDeleteRoom();
    void onSchedule();
    void onResultViewChanged();
    void onResultFilterChanged();
    void onResultSelectionChanged();
    void onCourseDoubleClicked(const QModelIndex& index);
    void onDosenDoubleClicked(const QModelIndex& index);
    void onRoomDoubleClicked(const QModelIndex& index);

private:
    void setupUi();
    void refreshCourseTable();
    void refreshDosenTable();
    void refreshRoomTable();
    void refreshResultTable();
    void refreshSummary();
    void refreshProdiFilter();
    void updateCounts();
    void updateDetail();
    // Penjelasan berbahasa sederhana untuk satu baris hasil: kenapa aman
    // atau bentrok, dan bentroknya dengan siapa.
    QString detailText(const ScheduleRow& row) const;
    void setDirty(bool dirty);
    void invalidateResults();
    // true = boleh lanjut (sudah disimpan / dibuang / memang bersih).
    bool confirmDiscard(const QString& aksi);
    bool saveToFile(const QString& path);
    bool loadFromFile(const QString& path);
    std::vector<std::string> distinctProdi() const;
    int selectedCourseIndex() const;
    int selectedDosenPosition() const;
    int selectedRoomIndex() const;
    bool kodeExists(const std::string& kode, int exceptIndex) const;
    std::vector<const ScheduleRow*> filteredResultRows() const;
    // Nama kanonis dosen dari ID; string kosong bila ID tidak dikenal.
    std::string dosenName(int dosenId) const;
    // Samakan salinan nama di tiap mata kuliah dengan daftar dosen (ID yang
    // menjadi patokan). Dipanggil setelah daftar dosen berubah.
    void syncDosenNames();
    // Tambah dosen bila namanya belum ada (perbandingan tanpa peduli huruf
    // besar/kecil); mengembalikan ID yang sudah ada atau yang baru dibuat.
    int addLecturerByName(const std::string& nama);
    int lecturerUsageCount(int dosenId) const;

    // Data yang diinput pengguna.
    std::vector<CourseData> m_courses;
    std::vector<Lecturer> m_dosens;
    int m_nextDosenId = 1;
    std::vector<RoomData> m_rooms;

    // Hasil penjadwalan terakhir (dihitung ulang tiap kali data berubah).
    ScheduleResult m_wpResult;
    ScheduleResult m_dinicResult;
    bool m_hasResult = false;
    bool m_resultsStale = false;

    bool m_dirty = false;
    QString m_currentFile;

    QTabWidget* m_tabs = nullptr;
    QTableView* m_courseView = nullptr;
    QStandardItemModel* m_courseModel = nullptr;
    QTableView* m_dosenView = nullptr;
    QStandardItemModel* m_dosenModel = nullptr;
    QTableView* m_roomView = nullptr;
    QStandardItemModel* m_roomModel = nullptr;
    QTableView* m_resultView = nullptr;
    QStandardItemModel* m_resultModel = nullptr;
    QComboBox* m_viewCombo = nullptr;
    QComboBox* m_prodiCombo = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QCheckBox* m_conflictOnlyCheck = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_detailLabel = nullptr;
    QLabel* m_countLabel = nullptr;
};
