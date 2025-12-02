#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace {
constexpr int kNameColumn = 0;
constexpr int kStartColumn = 1;
constexpr int kLengthColumn = 2;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupDatabase();
    loadTemplatesList();

    connect(ui->openFileButton, &QPushButton::clicked, this, &MainWindow::loadHexFromFile);
    connect(ui->decodeButton, &QPushButton::clicked, this, &MainWindow::decodeHex);
    connect(ui->addFieldButton, &QPushButton::clicked, this, &MainWindow::addFieldRow);
    connect(ui->removeFieldButton, &QPushButton::clicked, this, &MainWindow::removeSelectedFieldRows);
    connect(ui->saveTemplateButton, &QPushButton::clicked, this, &MainWindow::saveTemplate);
    connect(ui->loadTemplateButton, &QPushButton::clicked, this, &MainWindow::loadTemplateSelection);
    connect(ui->templateList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::templateSelectionChanged);

    addFieldRow();
}

MainWindow::~MainWindow()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    delete ui;
}

void MainWindow::setupDatabase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName("templates.db");
    if (!m_db.open()) {
        QMessageBox::warning(this, tr("База данных"), tr("Не удалось открыть базу: %1").arg(m_db.lastError().text()));
        return;
    }

    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS templates (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE)");
    query.exec("CREATE TABLE IF NOT EXISTS template_fields (template_id INTEGER, field_name TEXT, start_pos INTEGER, length INTEGER, ord INTEGER, "
               "FOREIGN KEY(template_id) REFERENCES templates(id) ON DELETE CASCADE)");
}

void MainWindow::loadTemplatesList()
{
    ui->templateList->clear();
    ui->templateList->addItem(tr("-- выбрать шаблон --"));
    if (!m_db.isOpen()) {
        return;
    }

    QSqlQuery query("SELECT name FROM templates ORDER BY name");
    while (query.next()) {
        ui->templateList->addItem(query.value(0).toString());
    }
}

void MainWindow::addFieldRow()
{
    const int row = ui->fieldsTable->rowCount();
    ui->fieldsTable->insertRow(row);
    ui->fieldsTable->setItem(row, kNameColumn, new QTableWidgetItem(tr("Поле %1").arg(row + 1)));
    ui->fieldsTable->setItem(row, kStartColumn, new QTableWidgetItem(QString::number(row * 8 + 1)));
    ui->fieldsTable->setItem(row, kLengthColumn, new QTableWidgetItem(QString::number(8)));
}

void MainWindow::removeSelectedFieldRows()
{
    const auto selected = ui->fieldsTable->selectionModel()->selectedRows();
    for (int i = selected.count() - 1; i >= 0; --i) {
        ui->fieldsTable->removeRow(selected.at(i).row());
    }
}

QString MainWindow::readHexInput() const
{
    QString hex = ui->hexInput->toPlainText();
    hex.remove(' ');
    hex.remove('\n');
    hex.remove('\r');
    return hex.trimmed();
}

QString MainWindow::hexToBitString(const QString &hex) const
{
    QByteArray bytes = QByteArray::fromHex(hex.toLatin1());
    QString bits;
    bits.reserve(bytes.size() * 8);
    for (unsigned char c : bytes) {
        bits.append(QStringLiteral("%1").arg(c, 8, 2, QLatin1Char('0')));
    }
    return bits;
}

void MainWindow::loadHexFromFile()
{
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Открыть HEX файл"), QString(), tr("Text Files (*.txt);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Файл"), tr("Не удалось открыть файл"));
        return;
    }

    const QString contents = QString::fromUtf8(file.readAll());
    ui->hexInput->setPlainText(contents.trimmed());
}

void MainWindow::decodeHex()
{
    const QString rawHex = readHexInput();
    if (rawHex.isEmpty()) {
        QMessageBox::information(this, tr("HEX"), tr("Введите HEX или загрузите файл."));
        return;
    }

    if ((rawHex.size() % 2) != 0) {
        QMessageBox::warning(this, tr("HEX"), tr("Количество символов hex должно быть четным."));
        return;
    }

    QString bits = hexToBitString(rawHex);

    ui->resultTable->setRowCount(0);
    for (int row = 0; row < ui->fieldsTable->rowCount(); ++row) {
        QTableWidgetItem *nameItem = ui->fieldsTable->item(row, kNameColumn);
        QTableWidgetItem *startItem = ui->fieldsTable->item(row, kStartColumn);
        QTableWidgetItem *lengthItem = ui->fieldsTable->item(row, kLengthColumn);
        if (!nameItem || !startItem || !lengthItem) {
            continue;
        }

        const QString fieldName = nameItem->text();
        bool okStart = false;
        bool okLength = false;
        int start = startItem->text().toInt(&okStart);
        int length = lengthItem->text().toInt(&okLength);
        if (!okStart || !okLength || start <= 0 || length <= 0) {
            continue;
        }

        const int startIndex = start - 1;
        if (startIndex + length > bits.size()) {
            continue;
        }

        const QString chunk = bits.mid(startIndex, length);
        bool okValue = false;
        quint64 value = chunk.toULongLong(&okValue, 2);
        const int resultRow = ui->resultTable->rowCount();
        ui->resultTable->insertRow(resultRow);
        ui->resultTable->setItem(resultRow, 0, new QTableWidgetItem(fieldName));
        ui->resultTable->setItem(resultRow, 1, new QTableWidgetItem(chunk));
        ui->resultTable->setItem(resultRow, 2, new QTableWidgetItem(okValue ? QString::number(value) : QStringLiteral("-")));
    }
}

void MainWindow::saveTemplate()
{
    if (!m_db.isOpen()) {
        QMessageBox::warning(this, tr("База данных"), tr("База данных недоступна."));
        return;
    }

    const QString name = ui->templateName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::information(this, tr("Шаблон"), tr("Укажите название шаблона."));
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO templates(name) VALUES(:name) ON CONFLICT(name) DO NOTHING");
    query.bindValue(":name", name);
    if (!query.exec()) {
        QMessageBox::warning(this, tr("Шаблон"), tr("Не удалось сохранить шаблон: %1").arg(query.lastError().text()));
        return;
    }

    QSqlQuery selectId;
    selectId.prepare("SELECT id FROM templates WHERE name = :name");
    selectId.bindValue(":name", name);
    if (!selectId.exec() || !selectId.next()) {
        QMessageBox::warning(this, tr("Шаблон"), tr("Не удалось получить ID шаблона: %1").arg(selectId.lastError().text()));
        return;
    }

    const int templateId = selectId.value(0).toInt();
    QSqlQuery deleteFields;
    deleteFields.prepare("DELETE FROM template_fields WHERE template_id = :id");
    deleteFields.bindValue(":id", templateId);
    deleteFields.exec();

    QSqlQuery insertField;
    insertField.prepare("INSERT INTO template_fields(template_id, field_name, start_pos, length, ord) "
                        "VALUES(:tid, :fname, :start, :len, :ord)");

    for (int row = 0; row < ui->fieldsTable->rowCount(); ++row) {
        auto *nameItem = ui->fieldsTable->item(row, kNameColumn);
        auto *startItem = ui->fieldsTable->item(row, kStartColumn);
        auto *lengthItem = ui->fieldsTable->item(row, kLengthColumn);
        if (!nameItem || !startItem || !lengthItem) {
            continue;
        }

        bool okStart = false;
        bool okLength = false;
        const int start = startItem->text().toInt(&okStart);
        const int length = lengthItem->text().toInt(&okLength);
        if (!okStart || !okLength) {
            continue;
        }

        insertField.bindValue(":tid", templateId);
        insertField.bindValue(":fname", nameItem->text());
        insertField.bindValue(":start", start);
        insertField.bindValue(":len", length);
        insertField.bindValue(":ord", row);
        insertField.exec();
    }

    loadTemplatesList();
    QMessageBox::information(this, tr("Шаблон"), tr("Шаблон сохранен."));
}

void MainWindow::templateSelectionChanged(int index)
{
    if (index <= 0) {
        return;
    }
    const QString name = ui->templateList->itemText(index);
    loadTemplateByName(name);
}

void MainWindow::loadTemplateSelection()
{
    const QString name = ui->templateList->currentText();
    if (name.isEmpty() || ui->templateList->currentIndex() == 0) {
        QMessageBox::information(this, tr("Шаблон"), tr("Выберите шаблон из списка."));
        return;
    }
    loadTemplateByName(name);
}

void MainWindow::loadTemplateByName(const QString &name)
{
    if (!m_db.isOpen()) {
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT id FROM templates WHERE name = :name");
    query.bindValue(":name", name);
    if (!query.exec() || !query.next()) {
        QMessageBox::warning(this, tr("Шаблон"), tr("Не удалось загрузить шаблон."));
        return;
    }

    const int templateId = query.value(0).toInt();
    ui->templateName->setText(name);

    QSqlQuery fieldsQuery;
    fieldsQuery.prepare("SELECT field_name, start_pos, length FROM template_fields WHERE template_id = :id ORDER BY ord");
    fieldsQuery.bindValue(":id", templateId);
    ui->fieldsTable->setRowCount(0);

    if (fieldsQuery.exec()) {
        int row = 0;
        while (fieldsQuery.next()) {
            ui->fieldsTable->insertRow(row);
            ui->fieldsTable->setItem(row, kNameColumn, new QTableWidgetItem(fieldsQuery.value(0).toString()));
            ui->fieldsTable->setItem(row, kStartColumn, new QTableWidgetItem(fieldsQuery.value(1).toString()));
            ui->fieldsTable->setItem(row, kLengthColumn, new QTableWidgetItem(fieldsQuery.value(2).toString()));
            ++row;
        }
    }
}
