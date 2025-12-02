#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void loadHexFromFile();
    void saveTemplate();
    void loadTemplateSelection();
    void decodeHex();
    void addFieldRow();
    void removeSelectedFieldRows();
    void templateSelectionChanged(int index);

private:
    void setupDatabase();
    void loadTemplatesList();
    void loadTemplateByName(const QString &name);
    QString readHexInput() const;
    QString hexToBitString(const QString &hex) const;

    Ui::MainWindow *ui;
    QSqlDatabase m_db;
};
#endif // MAINWINDOW_H
